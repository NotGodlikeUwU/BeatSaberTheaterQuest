package org.bstheater.quest;

import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.zip.GZIPInputStream;
import org.schabi.newpipe.extractor.NewPipe;
import org.schabi.newpipe.extractor.MediaFormat;
import org.schabi.newpipe.extractor.downloader.*;
import org.schabi.newpipe.extractor.stream.*;
import org.schabi.newpipe.extractor.InfoItem;
import org.schabi.newpipe.extractor.search.SearchExtractor;
import com.grack.nanojson.JsonArray;
import com.grack.nanojson.JsonObject;
import com.grack.nanojson.JsonWriter;

/** Runs on the native download worker. No Unity or UI calls are made here. */
public final class VideoDownloader {
    public static volatile long downloadedBytes;
    public static volatile long totalBytes;
    public static volatile String phase = "Idle";
    private static volatile boolean cancelled;
    private static int readTimeout = 15000;
    private static long deadline;
    private static boolean initialized;
    private static String cookieFile;
    private static YoutubeCookies session;
    public static volatile String sessionStatus = "YouTube: anonymous";
    public static volatile boolean loginRequired;
    public static volatile int resolvedHeight, resolvedFps;
    public static volatile boolean resolvedHls;
    static String resolvedAudioUrl;
    static boolean resolvedAudioHls;
    private static final String USER_AGENT = "Mozilla/5.0 (Android 12; Mobile; rv:140.0) Gecko/140.0 Firefox/140.0";
    private static final String EXTRACTOR_USER_AGENT = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36";

    private VideoDownloader() { }
    public static void setCookieFile(String path) { cookieFile = path; }
    private static void loadSession() throws IOException {
        session = null;
        if (cookieFile == null || !new File(cookieFile).isFile()) { sessionStatus = "YouTube: anonymous (no youtube-cookies.txt)"; return; }
        try {
            YoutubeCookies imported = YoutubeCookies.read(new File(cookieFile), System.currentTimeMillis() / 1000);
            if (!imported.authenticated(System.currentTimeMillis() / 1000)) throw new IOException("youtube-cookies.txt has no unexpired YouTube login session");
            session = imported;
            sessionStatus = "YouTube: login cookies loaded (server acceptance unverified)";
        } catch (IOException error) { sessionStatus = "YouTube: cookie export invalid or expired"; loginRequired = true; throw error; }
    }
    public static void cancel() { cancelled = true; }
    public static void prepare() { cancelled = false; loginRequired = false; downloadedBytes = 0; totalBytes = 0; phase = "Resolving video"; }
    public static boolean isYoutube(String source) throws IOException {
        String host = checkedUrl(source).getHost().toLowerCase(Locale.ROOT);
        return host.equals("youtu.be") || host.equals("youtube.com") || host.endsWith(".youtube.com") || host.equals("youtube-nocookie.com") || host.endsWith(".youtube-nocookie.com");
    }
    static URL checkedUrl(String source) throws IOException {
        URL url = new URL(source);
        // Loopback HTTP is used by the integration tests; remote URLs require HTTPS.
        boolean loopback = url.getHost().equals("127.0.0.1") || url.getHost().equals("localhost");
        if (!url.getProtocol().equals("https") && !(loopback && url.getProtocol().equals("http"))) throw new IOException("Video URL must use HTTPS");
        if (url.getUserInfo() != null) throw new IOException("Credentials in video URL are unsupported");
        return url;
    }
    static void checkCancelled() throws IOException {
        if (cancelled || Thread.currentThread().isInterrupted()) throw new InterruptedIOException("Video download cancelled");
        if (deadline != 0 && System.nanoTime() > deadline) throw new InterruptedIOException("Video download timed out");
    }
    private static HttpURLConnection open(String source, String method, Map<String,List<String>> headers, byte[] data) throws IOException {
        URL url = checkedUrl(source);
        for (int redirect = 0; redirect < 8; redirect++) {
            checkCancelled();
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setConnectTimeout(readTimeout);
            conn.setReadTimeout(readTimeout);
            conn.setInstanceFollowRedirects(false);
            conn.setRequestMethod(method);
            conn.setRequestProperty("User-Agent",USER_AGENT);
            conn.setRequestProperty("Accept-Encoding","identity");
            if (headers != null) for (Map.Entry<String,List<String>> header : headers.entrySet()) {
                for (String value : header.getValue()) conn.addRequestProperty(header.getKey(),value);
            }
            // Recompute per destination, including redirects. Never send the account session to media CDNs/third parties.
            if (session != null) for (Map.Entry<String,List<String>> header : session.headers(url, System.currentTimeMillis() / 1000).entrySet())
                conn.setRequestProperty(header.getKey(), String.join(" ", header.getValue()));
            try {
                if (data != null) {
                    conn.setDoOutput(true);
                    try (OutputStream out = conn.getOutputStream()) { out.write(data); }
                }
                int code = conn.getResponseCode();
                if (session != null && YoutubeCookies.youtubeHost(url.getHost().toLowerCase(Locale.ROOT)))
                    session.receive(url,conn.getHeaderFields(),System.currentTimeMillis()/1000,cookieFile);
                if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
                    String location = conn.getHeaderField("Location");
                    if (location == null) throw new IOException("Redirect missing Location");
                    URL next = checkedUrl(new URL(url,location).toString());
                    conn.disconnect();
                    if (!next.getHost().equalsIgnoreCase(url.getHost())) headers = null;
                    if (code == 303 || ((code == 301 || code == 302) && method.equals("POST"))) { method = "GET"; data = null; }
                    url = next;
                    continue;
                }
                return conn;
            } catch (IOException error) { conn.disconnect(); throw error; }
        }
        throw new IOException("Too many redirects");
    }
    private static InputStream responseStream(HttpURLConnection conn) throws IOException {
        InputStream input = conn.getResponseCode() >= 400 ? conn.getErrorStream() : conn.getInputStream();
        if (input == null) return new ByteArrayInputStream(new byte[0]);
        if ("gzip".equalsIgnoreCase(conn.getContentEncoding())) return new GZIPInputStream(input);
        return input;
    }
    static void verifyMedia(String media) throws IOException {
        HttpURLConnection connection = open(media, "GET", Collections.singletonMap("Range", Collections.singletonList("bytes=0-31")), null);
        try {
            int code = connection.getResponseCode();
            if (code != 200 && code != 206) throw new IOException("YouTube media endpoint returned HTTP " + code);
            try (InputStream input = responseStream(connection)) {
                if (input.readNBytes(16).length != 16) throw new IOException("YouTube returned empty media");
            }
        } finally { connection.disconnect(); }
    }
    private static final class ExtractorDownloader extends Downloader {
        @Override public Response execute(Request request) throws IOException {
            Map<String,List<String>> headers=new LinkedHashMap<>(request.headers());
            // NewPipe's WEB bootstrap/search parser requires desktop HTML. A
            // mobile default redirects the empty results page to m.youtube.com.
            if(headers.keySet().stream().noneMatch(key->key.equalsIgnoreCase("User-Agent")))
                headers.put("User-Agent",Collections.singletonList(EXTRACTOR_USER_AGENT));
            HttpURLConnection conn = open(request.url(),request.httpMethod(),headers,request.dataToSend());
            try {
                ByteArrayOutputStream body = new ByteArrayOutputStream();
                try (InputStream in = responseStream(conn)) {
                    byte[] buffer = new byte[32768]; int n;
                    while ((n = in.read(buffer)) != -1) {
                        checkCancelled();
                        if (body.size() + n > 8 * 1024 * 1024) throw new IOException("Extractor response exceeds 8 MiB");
                        body.write(buffer,0,n);
                    }
                }
                return new Response(conn.getResponseCode(),conn.getResponseMessage(),conn.getHeaderFields(),new String(body.toByteArray(),StandardCharsets.UTF_8),conn.getURL().toString());
            } finally { conn.disconnect(); }
        }
    }
    private static void initialize() {
        if (!initialized) { NewPipe.init(new ExtractorDownloader()); initialized = true; }
    }
    public static String search(String query) throws IOException {
        if(query==null || query.trim().isEmpty() || query.length()>512)throw new IOException("Enter a YouTube search query");
        prepare();initialize();loadSession();
        deadline=System.nanoTime()+60_000_000_000L;
        JsonArray results=new JsonArray();
        try {
            if(query.startsWith("https://")) {
                if(!isYoutube(query))throw new IOException("Search accepts YouTube URLs only");
                StreamExtractor extractor=NewPipe.getServiceByUrl(query).getStreamExtractor(query);extractor.fetchPage();
                JsonObject item=new JsonObject();item.put("url",extractor.getUrl());item.put("title",extractor.getName());
                item.put("author",extractor.getUploaderName());item.put("duration",(int)Math.max(0,Math.min(86400,extractor.getLength())));
                item.put("thumbnail",extractor.getThumbnails().isEmpty()?"":extractor.getThumbnails().get(0).getUrl());results.add(item);
            } else {
                SearchExtractor extractor=NewPipe.getService(0).getSearchExtractor(query.trim());extractor.fetchPage();
                for(InfoItem value:extractor.getInitialPage().getItems()) {
                    checkCancelled();if(!(value instanceof StreamInfoItem))continue;
                    StreamInfoItem video=(StreamInfoItem)value;JsonObject item=new JsonObject();
                    item.put("url",video.getUrl());item.put("title",video.getName());item.put("author",video.getUploaderName());
                    item.put("duration",(int)Math.max(0,Math.min(86400,video.getDuration())));
                    item.put("thumbnail",video.getThumbnails().isEmpty()?"":video.getThumbnails().get(0).getUrl());results.add(item);
                    if(results.size()==20)break;
                }
            }
            return JsonWriter.string(results);
        }catch(IOException error){throw error;}
        catch(Exception error){throw new IOException("YouTube search failed: "+error.getMessage(),error);}
        finally{deadline=0;}
    }
    static int height(VideoStream stream) {
        if (stream.getHeight() > 0) return stream.getHeight();
        String resolution = stream.getResolution();
        if (resolution == null) return 0;
        String digits = resolution.split("[^0-9]",2)[0];
        try { return Integer.parseInt(digits); } catch (NumberFormatException e) { return 0; }
    }
    static VideoStream choose(List<VideoStream> streams, int maxHeight) throws IOException {
        VideoStream best = null;
        for (VideoStream stream : streams) {
            if (stream.getFormat() != MediaFormat.MPEG_4 || !stream.isUrl() || stream.getDeliveryMethod() != DeliveryMethod.PROGRESSIVE_HTTP) continue;
            String codec = stream.getCodec();
            if (codec != null && !codec.isEmpty() && !codec.startsWith("avc") && !codec.toLowerCase(Locale.ROOT).contains("h264")) continue;
            int h = height(stream);
            if (h <= 0 || h > maxHeight) continue;
            if (best == null || h > height(best) || h == height(best) && (stream.getFps()>best.getFps()
                || stream.getFps()==best.getFps() && stream.isVideoOnly() && !best.isVideoOnly())) best = stream;
        }
        if (best == null) throw new IOException("No downloadable H.264 MP4 stream <= " + maxHeight + "p; this video may be restricted or require an extractor update");
        return best;
    }
    public static String resolve(String source, int maxHeight) throws IOException {
        resolvedHls=false;resolvedHeight=resolvedFps=0;resolvedAudioUrl=null;resolvedAudioHls=false;
        checkedUrl(source);
        if (!isYoutube(source)) return source;
        initialize();
        loginRequired = false;
        loadSession();
        if (session != null) {
            try {
                String id = NewPipe.getServiceByUrl(source).getStreamExtractor(source).getId();
                String media = AuthenticatedYoutube.resolve(new ExtractorDownloader(), id, maxHeight);
                sessionStatus = "YouTube: signed-in player request accepted";
                return media;
            } catch (IOException error) {
                if (error instanceof AuthenticatedYoutube.LoginRequiredException) { loginRequired = true; sessionStatus = "YouTube: session rejected"; }
                throw error;
            }
            catch (Exception error) { throw new IOException("YouTube authenticated extraction failed", error); }
        }
        try {
            StreamExtractor extractor = NewPipe.getServiceByUrl(source).getStreamExtractor(source);
            extractor.fetchPage();
            List<VideoStream> streams = new ArrayList<>(extractor.getVideoOnlyStreams());
            streams.addAll(extractor.getVideoStreams());
            VideoStream best=choose(streams,maxHeight);
            resolvedHeight=height(best);resolvedFps=best.getFps();
            AudioStream audio=null;
            for(AudioStream candidate:extractor.getAudioStreams())
                if(candidate.isUrl()&&candidate.getDeliveryMethod()==DeliveryMethod.PROGRESSIVE_HTTP&&candidate.getFormat()==MediaFormat.M4A
                    &&(audio==null||candidate.getAverageBitrate()>audio.getAverageBitrate()))audio=candidate;
            if(audio!=null)resolvedAudioUrl=audio.getContent();
            return best.getContent();
        } catch (IOException error) { throw error; }
        catch (org.schabi.newpipe.extractor.exceptions.SignInConfirmNotBotException error) {
            loginRequired = true;
            throw new IOException("YouTube requires login. Import youtube-cookies.txt for Theater; logging into Quest Browser does not sign in this downloader.", error);
        }
        catch (Exception error) { throw new IOException("YouTube extraction failed: " + error.getMessage(),error); }
    }
    static String fetchText(String url) throws IOException {
        return HlsVideo.fetch(new ExtractorDownloader(),url);
    }
    static void copyMedia(String url,OutputStream output,long limit) throws IOException {
        HttpURLConnection connection=open(url,"GET",null,null);
        try {
            if(connection.getResponseCode()!=200)throw new IOException("Video server returned HTTP "+connection.getResponseCode());
            try(InputStream input=responseStream(connection)) {
                byte[] buffer=new byte[131072];int size;
                while((size=input.read(buffer))!=-1) {
                    checkCancelled();
                    synchronized(VideoDownloader.class) {if(downloadedBytes+size>limit)throw new IOException("Video exceeds configured size limit");downloadedBytes+=size;}
                    output.write(buffer,0,size);
                }
            }
        }finally{connection.disconnect();}
    }
    // Validate complete top-level MP4 boxes before publishing the cache entry.
    public static void validateMp4(File file) throws IOException {
        boolean ftyp = false, moov = false, mdat = false;
        try (RandomAccessFile in = new RandomAccessFile(file,"r")) {
            long length = in.length(), offset = 0;
            while (offset < length) {
                if (length - offset < 8) throw new IOException("Truncated MP4 box header");
                in.seek(offset);
                long size = Integer.toUnsignedLong(in.readInt());
                byte[] type = new byte[4]; in.readFully(type);
                String name = new String(type,StandardCharsets.US_ASCII);
                long header = 8;
                if (size == 1) { if (length - offset < 16) throw new IOException("Truncated MP4 extended box"); size = in.readLong(); header = 16; }
                if (size == 0) size = length - offset;
                if (size < header || size > length - offset) throw new IOException("Truncated or invalid MP4 box");
                ftyp |= name.equals("ftyp"); moov |= name.equals("moov"); mdat |= name.equals("mdat");
                offset += size;
            }
        }
        if (!ftyp || !moov || !mdat) throw new IOException("Downloaded file is not a complete MP4 (ftyp/moov/mdat missing)");
    }
    public static void download(String source, String outputPath, int maxHeight, int timeoutSeconds, long limitBytes) throws IOException {
        downloadedBytes = 0; totalBytes = 0;
        deadline = System.nanoTime() + timeoutSeconds * 1_000_000_000L;
        File destination = new File(outputPath).getCanonicalFile();
        File partial = new File(outputPath + ".part").getCanonicalFile();
        File audioPartial=new File(outputPath+".audio.part"),combined=new File(outputPath+".av.part");
        if (!partial.getParentFile().equals(destination.getParentFile())) throw new IOException("Unsafe partial file path");
        boolean youtube=isYoutube(source);
        if (destination.exists()) {
            validateMp4(destination);
            // A cached fallback (e.g. 240p from TV) must not prevent a manual HD retry.
            if (!youtube) { phase = "Already downloaded"; deadline=0; return; }
        }
        phase = "Resolving video";
        try {
            String media = resolve(source,maxHeight);
            checkCancelled();
            if (youtube && destination.exists()) {
                int[] old=HlsVideo.dimensions(destination);
                if(old[0]>resolvedHeight || old[0]==resolvedHeight && old[1]>=resolvedFps && old[1]>0) {
                    saveQuality(destination,maxHeight,old[0],old[1]);phase="Best available video already downloaded";return;
                }
            }
            if(resolvedHls) HlsVideo.download(media,partial,limitBytes);
            else {
            phase = "Downloading MP4";
            HttpURLConnection conn = open(media,"GET",null,null);
            try {
                int code = conn.getResponseCode();
                if (code != 200) throw new IOException("Video server returned HTTP " + code);
                totalBytes = conn.getContentLengthLong();
                if (totalBytes > limitBytes) throw new IOException("Video exceeds configured size limit");
                if (totalBytes > 0 && destination.getParentFile().getUsableSpace() < totalBytes + 32 * 1024 * 1024L) throw new IOException("Not enough free storage for video");
                try (InputStream input = responseStream(conn); FileOutputStream output = new FileOutputStream(partial)) {
                    byte[] buffer = new byte[131072]; int n;
                    while ((n = input.read(buffer)) != -1) {
                        checkCancelled();
                        if (downloadedBytes + n > limitBytes) throw new IOException("Video exceeds configured size limit");
                        output.write(buffer,0,n); downloadedBytes += n;
                    }
                    output.getFD().sync();
                }
                if (totalBytes >= 0 && downloadedBytes != totalBytes) throw new IOException("Incomplete video response");
            } finally { conn.disconnect(); }
            }
            validateMp4(partial);
            if(youtube&&resolvedAudioUrl!=null) {
                try {
                    if(!Mp4Audio.hasAudio(partial)) {
                    phase="Downloading preview audio";
                    if(resolvedAudioHls)HlsVideo.download(resolvedAudioUrl,audioPartial,limitBytes);
                    else try(FileOutputStream audio=new FileOutputStream(audioPartial)){copyMedia(resolvedAudioUrl,audio,limitBytes);audio.getFD().sync();}
                    phase="Saving video and preview audio";Mp4Audio.mux(partial,audioPartial,combined,limitBytes);validateMp4(combined);
                    java.nio.file.Files.move(combined.toPath(),partial.toPath(),java.nio.file.StandardCopyOption.REPLACE_EXISTING);
                    }
                }catch(InterruptedIOException error){throw error;}
                catch(IOException | RuntimeException error){phase="Video ready; preview audio was unavailable";if(combined.exists())combined.delete();}
                finally{if(audioPartial.exists())audioPartial.delete();}
            }
            checkCancelled();
            // Same-folder rename makes completed videos visible atomically to native playback.
            try { java.nio.file.Files.move(partial.toPath(),destination.toPath(),java.nio.file.StandardCopyOption.ATOMIC_MOVE,java.nio.file.StandardCopyOption.REPLACE_EXISTING); }
            catch(java.nio.file.AtomicMoveNotSupportedException error) { java.nio.file.Files.move(partial.toPath(),destination.toPath(),java.nio.file.StandardCopyOption.REPLACE_EXISTING); }
            if(youtube)saveQuality(destination,maxHeight,resolvedHeight,resolvedFps);
            phase = "Video downloaded";
        } catch (IOException | RuntimeException error) {
            if (partial.exists() && !partial.delete()) partial.deleteOnExit();
            phase = "Failed: " + error.getMessage();
            throw error;
        } finally { deadline = 0; if(audioPartial.exists())audioPartial.delete();if(combined.exists())combined.delete(); }
    }
    private static void saveQuality(File video,int requested,int height,int fps) throws IOException {
        File file=new File(video+".bstheater-quality"),temporary=new File(file+".part");
        try(FileOutputStream output=new FileOutputStream(temporary)) {
            output.write((requested+" "+height+" "+fps+"\n").getBytes(StandardCharsets.US_ASCII));output.getFD().sync();
        }
        java.nio.file.Files.move(temporary.toPath(),file.toPath(),java.nio.file.StandardCopyOption.REPLACE_EXISTING);
    }
}
