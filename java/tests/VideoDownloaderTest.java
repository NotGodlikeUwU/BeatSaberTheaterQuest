package org.bstheater.quest;
import com.sun.net.httpserver.HttpServer;
import java.net.*;
import java.nio.file.*;
import java.io.*;
import java.util.*;
import org.schabi.newpipe.extractor.MediaFormat;
import org.schabi.newpipe.extractor.stream.*;

public final class VideoDownloaderTest {
    static int checks;
    static void check(boolean value,String reason) { checks++; if (!value) throw new AssertionError(reason); }
    static byte[] mp4() throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(bytes);
        out.writeInt(16); out.writeBytes("ftyp"); out.writeBytes("isom"); out.writeInt(0);
        out.writeInt(8); out.writeBytes("moov");
        out.writeInt(65544); out.writeBytes("mdat"); out.write(new byte[65536]);
        return bytes.toByteArray();
    }
    static VideoStream stream(String name,MediaFormat format,boolean videoOnly,DeliveryMethod delivery) {
        return new VideoStream.Builder().setId(name).setContent("https://example.org/video.mp4",true).setMediaFormat(format).setIsVideoOnly(videoOnly).setResolution(name).setDeliveryMethod(delivery).build();
    }
    static void fails(RunnableIO action,String reason) throws Exception {
        boolean failed=false;
        try { action.run(); } catch(IOException e) { failed=true; }
        check(failed,reason);
    }
    interface RunnableIO { void run() throws Exception; }
    public static void main(String[] args) throws Exception {
        if (args.length > 0 && args[0].equals("--online")) {
            long start=System.currentTimeMillis();
            String media=VideoDownloader.resolve(args[1],720);
            check(!media.equals(args[1]),"YouTube URL was not resolved");
            // Check the extracted media endpoint without downloading the entire music video.
            HttpURLConnection c=(HttpURLConnection)new URL(media).openConnection();
            c.setConnectTimeout(15000); c.setReadTimeout(15000); c.setRequestProperty("Range","bytes=0-1023");
            int code=c.getResponseCode();
            check(code==200||code==206,"Resolved media returned HTTP "+code);
            try(InputStream in=c.getInputStream()) { check(in.readNBytes(16).length==16,"Empty media body"); }
            finally { c.disconnect(); }
            System.out.println("YouTube extraction and media request passed in "+(System.currentTimeMillis()-start)+"ms");
            return;
        }
        Path folder=Paths.get(args[0]); Files.createDirectories(folder);
        byte[] media=mp4();
        HttpServer server=HttpServer.create(new InetSocketAddress("127.0.0.1",0),0);
        server.createContext("/clip",exchange->{ exchange.getResponseHeaders().set("Content-Type","video/mp4"); exchange.sendResponseHeaders(200,media.length); exchange.getResponseBody().write(media); exchange.close(); });
        server.createContext("/redirect",exchange->{ exchange.getResponseHeaders().set("Location","/clip"); exchange.sendResponseHeaders(302,-1); exchange.close(); });
        server.createContext("/loop",exchange->{ exchange.getResponseHeaders().set("Location","/loop"); exchange.sendResponseHeaders(302,-1); exchange.close(); });
        server.createContext("/html",exchange->{ byte[] html="<html>login</html>".getBytes(); exchange.sendResponseHeaders(200,html.length); exchange.getResponseBody().write(html); exchange.close(); });
        server.createContext("/denied",exchange->{ exchange.sendResponseHeaders(403,-1); exchange.close(); });
        server.createContext("/slow",exchange->{
            try {
                exchange.sendResponseHeaders(200,media.length); exchange.getResponseBody().write(media,0,32); exchange.getResponseBody().flush();
                try { Thread.sleep(300); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
                exchange.getResponseBody().write(media,32,media.length-32);
            } catch (IOException ignored) { } finally { exchange.close(); }
        });
        server.start();
        String base="http://127.0.0.1:"+server.getAddress().getPort();
        try {
            VideoDownloader.verifyMedia(base+"/clip");
            check(true,"Media preflight failed");
            fails(()->VideoDownloader.verifyMedia(base+"/denied"),"Media preflight accepted HTTP 403");
            File output=folder.resolve("音楽-🎵.mp4").toFile();
            VideoDownloader.download(base+"/redirect",output.toString(),720,30,1024*1024);
            check(output.exists(),"Completed file missing");
            check(Arrays.equals(Files.readAllBytes(output.toPath()),media),"Downloaded content mismatch");
            check(!new File(output+".part").exists(),"Partial file left after success");
            check(VideoDownloader.downloadedBytes==media.length,"Progress bytes wrong");
            VideoDownloader.download(base+"/denied",output.toString(),720,30,1024*1024);
            check(Arrays.equals(Files.readAllBytes(output.toPath()),media),"Existing media overwritten");
            File bad=folder.resolve("bad.mp4").toFile();
            fails(()->VideoDownloader.download(base+"/html",bad.toString(),720,30,1024*1024),"HTML accepted as MP4");
            check(!bad.exists()&&!new File(bad+".part").exists(),"Failed media published or partial leaked");
            fails(()->VideoDownloader.download(base+"/denied",bad.toString(),720,30,1024*1024),"HTTP 403 accepted");
            fails(()->VideoDownloader.download(base+"/loop",bad.toString(),720,30,1024*1024),"Redirect loop accepted");
            fails(()->VideoDownloader.download(base+"/clip",bad.toString(),720,30,1024),"Size limit ignored");
            check(!bad.exists()&&!new File(bad+".part").exists(),"Oversize file leaked");
            fails(()->VideoDownloader.download("file:///etc/passwd",bad.toString(),720,30,1024*1024),"Non-HTTP URL accepted");
            byte[] truncated=Arrays.copyOf(media,media.length-1); Files.write(bad.toPath(),truncated);
            fails(()->VideoDownloader.validateMp4(bad),"Truncated MP4 accepted");
            check(VideoDownloader.isYoutube("https://www.youtube.com/watch?v=krdQLzzAeZU"),"YouTube host missing");
            check(!VideoDownloader.isYoutube("https://youtube.com.example.org/video.mp4"),"YouTube hostname spoof accepted");
            VideoStream low=stream("360p",MediaFormat.MPEG_4,false,DeliveryMethod.PROGRESSIVE_HTTP);
            VideoStream high=stream("720p",MediaFormat.MPEG_4,true,DeliveryMethod.PROGRESSIVE_HTTP);
            VideoStream tooHigh=stream("1080p",MediaFormat.MPEG_4,true,DeliveryMethod.PROGRESSIVE_HTTP);
            VideoStream webm=stream("720p",MediaFormat.WEBM,true,DeliveryMethod.PROGRESSIVE_HTTP);
            check(VideoDownloader.choose(Arrays.asList(low,high,tooHigh,webm),720)==high,"Quality/format selection failed");
            check(VideoDownloader.choose(Arrays.asList(low,tooHigh),720)==low,"Lower-resolution fallback failed");
            fails(()->VideoDownloader.choose(Collections.singletonList(webm),720),"WebM accepted");
            fails(()->VideoDownloader.choose(Collections.singletonList(stream("720p",MediaFormat.MPEG_4,true,DeliveryMethod.HLS)),720),"HLS manifest accepted as MP4");
            File cancelled=folder.resolve("cancelled.mp4").toFile();
            Thread cancelThread=new Thread(()-> { try { Thread.sleep(50); } catch(InterruptedException ignored) { } VideoDownloader.cancel(); });
            VideoDownloader.prepare(); cancelThread.start();
            fails(()->VideoDownloader.download(base+"/slow",cancelled.toString(),720,30,1024*1024),"Cancellation ignored");
            cancelThread.join();
            check(!cancelled.exists()&&!new File(cancelled+".part").exists(),"Cancelled file published or partial leaked");
            VideoDownloader.prepare();
            VideoDownloader.download(base+"/clip",cancelled.toString(),720,30,1024*1024);
            check(cancelled.exists(),"Retry after cancellation failed");
            System.out.println(checks+" Java download checks passed");
        } finally { server.stop(0); }
    }
}
