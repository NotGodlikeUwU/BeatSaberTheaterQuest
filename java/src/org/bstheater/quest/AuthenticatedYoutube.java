package org.bstheater.quest;

import com.grack.nanojson.*;
import java.io.IOException;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import org.schabi.newpipe.extractor.downloader.*;
import org.schabi.newpipe.extractor.services.youtube.*;
import org.schabi.newpipe.extractor.MediaFormat;
import org.schabi.newpipe.extractor.stream.*;

/** Cookie-capable clients and SAPISID hashes follow yt-dlp's authenticated path. */
final class AuthenticatedYoutube {
    static final List<String> diagnostics = new ArrayList<>();
    static final class LoginRequiredException extends IOException {
        LoginRequiredException() { super("YouTube rejected this session. Export fresh youtube-cookies.txt and retry."); }
    }
    private static final String ENDPOINT = "https://www.youtube.com/youtubei/v1/player?prettyPrint=false";
    static String resolve(Downloader downloader, String videoId, int maxHeight) throws IOException {
        Integer timestamp = null;
        try { timestamp = YoutubeJavaScriptPlayerManager.getSignatureTimestamp(videoId); }
        catch (Exception ignored) { /* Some direct TV formats don't require player JavaScript. */ }
        return resolveWithTimestamp(downloader, videoId, maxHeight, timestamp);
    }
    static String resolveWithTimestamp(Downloader downloader, String videoId, int maxHeight, Integer timestamp) throws IOException {
        return resolveWithTimestamp(downloader,videoId,maxHeight,timestamp,VideoDownloader::verifyMedia);
    }
    interface MediaVerifier { void verify(String url) throws IOException; }
    static String resolveWithTimestamp(Downloader downloader, String videoId, int maxHeight, Integer timestamp, MediaVerifier verifier) throws IOException {
        diagnostics.clear();
        String cpn = YoutubeParsingHelper.generateContentPlaybackNonce();
        String audioUrl=null;boolean audioHls=false;
        // Same cookie-capable client family as PC yt-dlp; the anonymous Android client ignores login cookies.
        String[][] clients = {
            {"TVHTML5", "7.20260707.07.00", "7", "Mozilla/5.0 (ChromiumStylePlatform) Cobalt/25.lts.30.1034943-gold (unlike Gecko), Unknown_TV_Unknown_0/Unknown (Unknown, Unknown)"},
            {"TVHTML5", "5.20260707", "7", "Mozilla/5.0 (ChromiumStylePlatform) Cobalt/Version"},
            {"WEB_EMBEDDED_PLAYER", "2.20260708.00.00", "56", "Mozilla/5.0 (Android 12) AppleWebKit/537.36 Chrome/140.0.0.0 Safari/537.36"},
            {"WEB", "2.20260708.00.00", "1", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.5 Safari/605.1.15"}
        };
        IOException last = null;
        boolean accepted = false, loginDenied = false;
        String fallback=null;int fallbackHeight=0,fallbackFps=0;boolean fallbackHls=false;
        for (String[] client : clients) {
            VideoDownloader.checkCancelled();
            try {
                JsonBuilder<JsonObject> builder = JsonObject.builder().object("context").object("client")
                    .value("clientName", client[0]).value("clientVersion", client[1]).value("hl", "en").value("gl", "US")
                    .value("userAgent", client[3]).end();
                if (client[0].equals("WEB_EMBEDDED_PLAYER")) builder.object("thirdParty")
                    .value("embedUrl", "https://www.youtube.com/watch?v=" + videoId).end();
                builder.end().value("videoId", videoId).value("cpn", cpn)
                    .value("contentCheckOk", true).value("racyCheckOk", true);
                if (timestamp != null) builder.object("playbackContext").object("contentPlaybackContext")
                    .value("signatureTimestamp", timestamp).value("html5Preference","HTML5_PREF_WANTS").end().end();
                byte[] body = JsonWriter.string(builder.done()).getBytes(StandardCharsets.UTF_8);
                Map<String, List<String>> headers = new LinkedHashMap<>();
                headers.put("Content-Type", Collections.singletonList("application/json"));
                headers.put("User-Agent", Collections.singletonList(client[3]));
                headers.put("X-YouTube-Client-Name", Collections.singletonList(client[2]));
                headers.put("X-YouTube-Client-Version", Collections.singletonList(client[1]));
                Response response = downloader.post(ENDPOINT, headers, body);
                if (response.responseCode() != 200) throw new IOException("YouTube login request returned HTTP " + response.responseCode());
                JsonObject player = JsonParser.object().from(response.responseBody());
                if (player.getObject("playabilityStatus").getString("status","").equals("OK")) accepted=true;
                if(player.getObject("playabilityStatus").getString("status","").equals("OK")) {
                    String directAudio=chooseAudio(player,videoId,cpn,verifier);
                    if(directAudio!=null){audioUrl=directAudio;audioHls=false;}
                }
                String manifest=player.getObject("streamingData").getString("hlsManifestUrl");
                if(manifest!=null && player.getObject("playabilityStatus").getString("status","").equals("OK"))try {
                    HlsVideo.Rendition rendition=HlsVideo.choose(downloader,PlayerSolver.decodeManifest(videoId,manifest),maxHeight,verifier);
                    if(audioUrl==null&&rendition.audioUrl!=null){audioUrl=rendition.audioUrl;audioHls=true;}
                    if(rendition.height>fallbackHeight || rendition.height==fallbackHeight && rendition.fps>fallbackFps) {
                        fallback=rendition.url;fallbackHeight=rendition.height;fallbackFps=rendition.fps;fallbackHls=true;
                        if(rendition.height>=maxHeight && rendition.fps>=60) {
                            VideoDownloader.resolvedHls=true;VideoDownloader.resolvedHeight=rendition.height;VideoDownloader.resolvedFps=rendition.fps;
                            VideoDownloader.resolvedAudioUrl=audioUrl;VideoDownloader.resolvedAudioHls=audioHls;
                            return rendition.url;
                        }
                    }
                }catch(IOException error){diagnostics.add("HLS: "+error.getMessage().replaceAll("https?://\\S+","[URL]"));}
                diagnostics.add(client[0]+": status="+player.getObject("playabilityStatus").getString("status", "ERROR")+", HLS="+(player.getObject("streamingData").getString("hlsManifestUrl")!=null));
                String media=chooseResponse(player, videoId, maxHeight, cpn, verifier);
                int height=VideoDownloader.resolvedHeight,fps=VideoDownloader.resolvedFps;
                if(height>=maxHeight && fps>=60) {VideoDownloader.resolvedHls=false;VideoDownloader.resolvedAudioUrl=audioUrl;VideoDownloader.resolvedAudioHls=audioHls;return media;}
                if(height>fallbackHeight || height==fallbackHeight && fps>fallbackFps) { fallback=media;fallbackHeight=height;fallbackFps=fps;fallbackHls=false; }
            } catch (java.io.InterruptedIOException error) { throw error; }
            catch (IOException error) {
                if (error instanceof LoginRequiredException) loginDenied=true;
                diagnostics.add(client[0]+": "+error.getMessage().replaceAll("https?://\\S+","[URL]"));
                for (Throwable cause=error.getCause();cause!=null;cause=cause.getCause()) diagnostics.add("Decoder cause: "+cause.getClass().getSimpleName()+" at "+(cause.getStackTrace().length==0 ? "unknown" : cause.getStackTrace()[0].getClassName()+"."+cause.getStackTrace()[0].getMethodName()));
                if (!(error instanceof LoginRequiredException) || last==null) last = error;
            }
            catch (Exception error) { last = new IOException("YouTube authenticated response could not be parsed", error); }
        }
        if (fallback!=null) { VideoDownloader.resolvedHls=fallbackHls;VideoDownloader.resolvedHeight=fallbackHeight;VideoDownloader.resolvedFps=fallbackFps;VideoDownloader.resolvedAudioUrl=audioUrl;VideoDownloader.resolvedAudioHls=audioHls;return fallback; }
        if (!accepted && loginDenied) throw new LoginRequiredException();
        throw last == null ? new IOException("YouTube authenticated extraction failed") : last;
    }
    static String chooseResponse(JsonObject player, String videoId, int maxHeight, String cpn) throws IOException {
        return chooseResponse(player,videoId,maxHeight,cpn,url -> {});
    }
    static String chooseResponse(JsonObject player, String videoId, int maxHeight, String cpn, MediaVerifier verifier) throws IOException {
        String status = player.getObject("playabilityStatus").getString("status", "ERROR");
        if (!status.equals("OK")) {
            if (status.equals("LOGIN_REQUIRED")) throw new LoginRequiredException();
            throw new IOException("YouTube video unavailable for this session (" + status.replaceAll("[^A-Z_]", "") + ")");
        }
        if (!videoId.equals(player.getObject("videoDetails").getString("videoId"))) throw new IOException("YouTube returned a different video");
        List<JsonObject> formats = new ArrayList<>();
        JsonObject streaming = player.getObject("streamingData");
        for (String key : Arrays.asList("formats", "adaptiveFormats")) {
            for (Object value : streaming.getArray(key)) {
                if (!(value instanceof JsonObject)) continue;
                JsonObject format = (JsonObject) value;
                String mime = format.getString("mimeType", "");
                int height = format.getInt("height");
                if (!mime.startsWith("video/mp4") || !mime.contains("avc1") || height <= 0 || height > maxHeight
                    || !format.getArray("drmFamilies").isEmpty() || format.getString("type", "").contains("OTF")) continue;
                formats.add(format);
            }
        }
        formats.sort(Comparator.comparingInt((JsonObject f) -> f.getInt("height")).thenComparingInt(f -> f.getInt("fps")).reversed());
        IOException decodeError = null;
        for (JsonObject format : formats) {
            try {
                String url = format.getString("url");
                if (url == null) {
                    Map<String, String> cipher = query(format.getString("signatureCipher", format.getString("cipher", "")));
                    if (!cipher.containsKey("url") || !cipher.containsKey("s")) continue;
                    String signature;
                    try { signature=YoutubeJavaScriptPlayerManager.deobfuscateSignature(videoId,cipher.get("s")); }
                    catch (org.schabi.newpipe.extractor.exceptions.ParsingException error) { signature=PlayerSolver.solve(videoId,"sig",cipher.get("s")); }
                    url = cipher.get("url") + "&" + encode(cipher.getOrDefault("sp", "signature")) + "=" + encode(signature);
                }
                URL checked = VideoDownloader.checkedUrl(url);
                if (query(checked.getQuery() == null ? "" : checked.getQuery()).containsKey("n")) {
                    try { url = YoutubeJavaScriptPlayerManager.getUrlWithThrottlingParameterDeobfuscated(videoId, url); }
                    catch (org.schabi.newpipe.extractor.exceptions.ParsingException error) { url=PlayerSolver.decodeN(videoId,url); }
                }
                String media=url + (url.contains("?") ? "&" : "?") + "cpn=" + encode(cpn);
                verifier.verify(media);
                VideoDownloader.resolvedHeight=format.getInt("height");VideoDownloader.resolvedFps=format.getInt("fps");
                return media;
            } catch (java.io.InterruptedIOException error) { throw error; }
            catch (IOException error) { decodeError = error; }
            catch (Exception error) { decodeError = new IOException("YouTube player signature could not be decoded; extractor update required", error); }
        }
        if (decodeError != null) throw decodeError;
        throw new IOException("Signed-in YouTube returned no downloadable H.264 MP4 <= " + maxHeight + "p");
    }
    private static String encode(String value) { return URLEncoder.encode(value, StandardCharsets.UTF_8); }
    private static String chooseAudio(JsonObject player,String videoId,String cpn,MediaVerifier verifier) throws IOException {
        List<JsonObject> audio=new ArrayList<>();
        for(Object value:player.getObject("streamingData").getArray("adaptiveFormats"))if(value instanceof JsonObject) {
            JsonObject format=(JsonObject)value;
            if(format.getString("mimeType","").startsWith("audio/mp4")&&format.getString("mimeType","").contains("mp4a")
                &&format.getArray("drmFamilies").isEmpty()&&!format.getString("type","").contains("OTF"))audio.add(format);
        }
        audio.sort(Comparator.comparingInt((JsonObject f)->f.getInt("bitrate")).reversed());
        for(JsonObject format:audio)try {
            String url=format.getString("url");
            if(url==null) {
                Map<String,String> cipher=query(format.getString("signatureCipher",format.getString("cipher","")));
                if(!cipher.containsKey("url")||!cipher.containsKey("s"))continue;
                String signature;
                try{signature=YoutubeJavaScriptPlayerManager.deobfuscateSignature(videoId,cipher.get("s"));}
                catch(org.schabi.newpipe.extractor.exceptions.ParsingException error){signature=PlayerSolver.solve(videoId,"sig",cipher.get("s"));}
                url=cipher.get("url")+"&"+encode(cipher.getOrDefault("sp","signature"))+"="+encode(signature);
            }
            URL checked=VideoDownloader.checkedUrl(url);
            if(query(checked.getQuery()==null?"":checked.getQuery()).containsKey("n")) {
                try{url=YoutubeJavaScriptPlayerManager.getUrlWithThrottlingParameterDeobfuscated(videoId,url);}
                catch(org.schabi.newpipe.extractor.exceptions.ParsingException error){url=PlayerSolver.decodeN(videoId,url);}
            }
            url+=(url.contains("?")?"&":"?")+"cpn="+encode(cpn);verifier.verify(url);return url;
        }catch(java.io.InterruptedIOException error){throw error;}
        catch(Exception ignored){/* Video download remains usable when no AAC client is accepted. */}
        return null;
    }
    private static Map<String, String> query(String value) {
        Map<String, String> pairs = new LinkedHashMap<>();
        for (String pair : value.split("&")) {
            String[] parts = pair.split("=", 2);
            if (parts.length == 2) pairs.put(URLDecoder.decode(parts[0], StandardCharsets.UTF_8), URLDecoder.decode(parts[1], StandardCharsets.UTF_8));
        }
        return pairs;
    }
}

