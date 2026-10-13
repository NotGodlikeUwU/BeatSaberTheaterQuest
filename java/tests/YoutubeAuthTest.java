package org.bstheater.quest;

import com.grack.nanojson.*;
import java.io.*;
import java.net.*;
import java.nio.file.*;
import java.util.*;
import org.schabi.newpipe.extractor.downloader.*;

/** Synthetic sessions only. No real account credentials are included or printed. */
public final class YoutubeAuthTest {
    private static int checks;
    private static void check(boolean ok, String message) { checks++; if (!ok) throw new AssertionError(message); }
    private static void fails(VideoDownloaderTest.RunnableIO action, String message) throws Exception {
        boolean failed = false;
        try { action.run(); } catch (IOException error) { failed = true; check(!error.getMessage().contains("synthetic-sid"), "Secret leaked in error"); }
        check(failed, message);
    }
    static String export() {
        return "# Netscape HTTP Cookie File\n"
            + ".youtube.com\tTRUE\t/\tTRUE\t2000\tSAPISID\tsynthetic-sid\n"
            + "#HttpOnly_.youtube.com\tTRUE\t/\tTRUE\t2000\tLOGIN_INFO\tsynthetic-login\n"
            + ".google.com\tTRUE\t/\tTRUE\t2000\tOTHER_ACCOUNT\tnever-imported\n"
            + ".youtube.com\tTRUE\t/\tTRUE\t999\tEXPIRED\tnever-sent\n"
            + "www.youtube.com\tFALSE\t/watch\tTRUE\t0\tWATCH_ONLY\tonly-watch\n";
    }
    private static JsonObject response() throws Exception {
        return JsonParser.object().from("{\"playabilityStatus\":{\"status\":\"OK\"},\"videoDetails\":{\"videoId\":\"abcdefghijk\"},\"streamingData\":{\"adaptiveFormats\":["
            + "{\"mimeType\":\"video/webm; codecs=vp9\",\"height\":720,\"url\":\"https://example.org/webm\"},"
            + "{\"mimeType\":\"video/mp4; codecs=avc1\",\"height\":1080,\"url\":\"https://example.org/high\"},"
            + "{\"mimeType\":\"video/mp4; codecs=avc1\",\"height\":720,\"fps\":60,\"url\":\"https://example.org/720\"},"
            + "{\"mimeType\":\"video/mp4; codecs=avc1\",\"height\":360,\"url\":\"https://example.org/360\"}]}}");
    }
    public static void main(String[] args) throws Exception {
        YoutubeCookies cookies = YoutubeCookies.parse(export(), 1000);
        check(cookies.authenticated(1000), "Login session was not detected");
        Map<String,List<String>> api = cookies.headers(new URL("https://www.youtube.com/youtubei/v1/player"), 1000);
        check(api.get("Cookie").get(0).contains("LOGIN_INFO=synthetic-login"), "HttpOnly cookie import failed");
        check(!api.get("Cookie").get(0).contains("EXPIRED") && !api.get("Cookie").get(0).contains("OTHER_ACCOUNT"), "Expired/other account cookies imported");
        check(!api.get("Cookie").get(0).contains("WATCH_ONLY"), "Cookie path scope ignored");
        check(api.get("Authorization").get(0).equals("SAPISIDHASH 1000_9a2795b351af8353393c0801fb375569e8c44e39"), "SAPISID hash differs from expected algorithm");
        check(cookies.headers(new URL("https://rr1.googlevideo.com/videoplayback"), 1000).isEmpty(), "Session leaked to media CDN");
        check(cookies.headers(new URL("https://youtube.com.evil.invalid/youtubei/v1/player"), 1000).isEmpty(), "Session leaked to spoofed domain");
        check(cookies.headers(new URL("https://accounts.google.com/"), 1000).isEmpty(), "Session leaked to unrelated Google service");
        check(cookies.headers(new URL("http://www.youtube.com/youtubei/v1/player"), 1000).isEmpty(), "Session leaked over HTTP");
        check(cookies.headers(new URL("https://www.youtube.com/watch?v=abcdefghijk"), 1000).get("Cookie").get(0).contains("WATCH_ONLY"), "Cookie path match failed");
        check(!cookies.headers(new URL("https://www.youtube.com/watchlist"), 1000).get("Cookie").get(0).contains("WATCH_ONLY"), "Cookie path boundary ignored");
        check(!cookies.headers(new URL("https://www.youtube.com/watch?v=abcdefghijk"), 1000).containsKey("Authorization"), "API authorization sent to webpage");
        check(!cookies.authenticated(2001), "Expired session accepted");
        YoutubeCookies third = YoutubeCookies.parse(export().replace("\tSAPISID\t", "\t__Secure-3PAPISID\t"), 1000);
        check(third.headers(new URL("https://www.youtube.com/youtubei/v1/player"),1000).get("Authorization").get(0).contains("SAPISID3PHASH"), "Third-party SAPISID fallback failed");
        fails(() -> YoutubeCookies.parse("SAPISID=synthetic-sid",1000), "Raw/unscoped cookie header accepted");
        fails(() -> YoutubeCookies.parse(export().replace("synthetic-sid", "synthetic-sid; injected=yes"),1000), "Cookie header injection accepted");
        fails(() -> YoutubeCookies.parse(export().replace("2000\tSAPISID", "not-an-expiry\tSAPISID"),1000), "Invalid expiry accepted");
        Path scratch = Files.createDirectories(Paths.get(args[0]));
        Path rotated=scratch.resolve("synthetic-session.txt");Files.writeString(rotated,export());
        cookies.receive(new URL("https://www.youtube.com/youtubei/v1/player"),Collections.singletonMap("Set-Cookie",Collections.singletonList("SAPISID=rotated-synthetic-sid; Domain=.youtube.com; Path=/; Secure; Max-Age=3600")),1000,rotated.toString());
        check(YoutubeCookies.read(rotated.toFile(),1000).headers(new URL("https://www.youtube.com/"),1000).get("Cookie").get(0).contains("SAPISID=rotated-synthetic-sid"),"Rotated session was not saved");
        cookies.receive(new URL("https://rr1.googlevideo.com/"),Collections.singletonMap("Set-Cookie",Collections.singletonList("SAPISID=cdn-injection; Domain=.youtube.com; Path=/; Secure")),1000,rotated.toString());
        check(!cookies.headers(new URL("https://www.youtube.com/"),1000).get("Cookie").get(0).contains("cdn-injection"),"CDN changed the login session");
        Path huge = scratch.resolve("large-cookies.txt"); Files.write(huge, new byte[256*1024+1]);
        fails(() -> YoutubeCookies.read(huge.toFile(),1000), "Unbounded cookie file accepted");
        check(AuthenticatedYoutube.chooseResponse(response(),"abcdefghijk",720,"nonce").equals("https://example.org/720?cpn=nonce"), "Authenticated resolution selection failed");
        check(AuthenticatedYoutube.chooseResponse(response(),"abcdefghijk",480,"nonce").contains("/360?"), "Authenticated quality fallback failed");
        fails(() -> AuthenticatedYoutube.chooseResponse(response(),"wrongvideo1",720,"nonce"), "Wrong video response accepted");
        fails(() -> AuthenticatedYoutube.chooseResponse(response(),"abcdefghijk",144,"nonce"), "Unsupported authenticated format accepted");
        JsonObject denied = JsonParser.object().from("{\"playabilityStatus\":{\"status\":\"LOGIN_REQUIRED\"}}");
        fails(() -> AuthenticatedYoutube.chooseResponse(denied,"abcdefghijk",720,"nonce"), "Rejected session accepted");
        int[] requests = {0};
        Downloader fake = new Downloader() {
            @Override public Response execute(Request request) throws IOException {
                try {
                    requests[0]++;
                    check(request.url().equals("https://www.youtube.com/youtubei/v1/player?prettyPrint=false"), "Login request sent to wrong endpoint");
                    check(request.httpMethod().equals("POST"), "Wrong authenticated request method");
                    JsonObject body = JsonParser.object().from(new String(request.dataToSend(),java.nio.charset.StandardCharsets.UTF_8));
                    check(body.getString("videoId").equals("abcdefghijk"), "Player request changed video ID");
                    check(body.getObject("playbackContext").getObject("contentPlaybackContext").getInt("signatureTimestamp") == 1234, "Signature timestamp missing");
                    check(body.getObject("context").getObject("client").getString("clientName").equals("TVHTML5"), "Non-cookie client selected");
                    return new Response(200,"OK",Collections.emptyMap(),JsonWriter.string(requests[0] == 1 ? denied : response()),request.url());
                } catch (Exception error) { throw new IOException("Fake player test failed",error); }
            }
        };
        check(AuthenticatedYoutube.resolveWithTimestamp(fake,"abcdefghijk",720,1234,url -> {}).startsWith("https://example.org/720?cpn="), "Authenticated client fallback failed");
        check(requests[0] == 2, "Authenticated clients did not stop after successful response");
        int[] mediaChecks={0};
        String fallback=AuthenticatedYoutube.chooseResponse(response(),"abcdefghijk",720,"nonce",url -> {
            mediaChecks[0]++;
            if (url.contains("/720?")) throw new IOException("YouTube media endpoint returned HTTP 403");
        });
        check(fallback.contains("/360?") && mediaChecks[0]==2,"Rejected high-resolution media prevented valid lower-format download");
        Downloader blocked=new Downloader() {
            @Override public Response execute(Request request) { return new Response(200,"OK",Collections.emptyMap(),JsonWriter.string(denied),request.url()); }
        };
        boolean blockedCorrectly=false;
        try { AuthenticatedYoutube.resolveWithTimestamp(blocked,"abcdefghijk",720,1234,url -> {}); }
        catch (AuthenticatedYoutube.LoginRequiredException error) { blockedCorrectly=true; }
        check(blockedCorrectly,"Rejected login did not stop the download queue");
        int[] hlsClients={0};
        Downloader mixedHls=new Downloader() {
            @Override public Response execute(Request request) throws IOException {
                String body;
                if(request.httpMethod().equals("POST")) {
                    hlsClients[0]++;
                    String rendition=hlsClients[0]==4?"high":"low";
                    body="{\"playabilityStatus\":{\"status\":\"OK\"},\"videoDetails\":{\"videoId\":\"abcdefghijk\"},\"streamingData\":{\"hlsManifestUrl\":\"https://example.org/"+rendition+"-master\"}}";
                } else if(request.url().endsWith("-master")) {
                    boolean high=request.url().contains("high");
                    body="#EXTM3U\n#EXT-X-STREAM-INF:RESOLUTION="+(high?"1920x1080":"426x240")+",FRAME-RATE=24,CODECS=\"avc1.640028\"\n"+(high?"high":"low")+"-playlist\n";
                } else body="#EXTM3U\n#EXTINF:1,\nsegment.ts\n#EXT-X-ENDLIST\n";
                return new Response(200,"OK",Collections.emptyMap(),body,request.url());
            }
        };
        check(AuthenticatedYoutube.resolveWithTimestamp(mixedHls,"abcdefghijk",1080,1234,url -> {}).contains("high-playlist"),"Early low-quality HLS prevented checking WEB HD");
        check(hlsClients[0]==4 && VideoDownloader.resolvedHls && VideoDownloader.resolvedHeight==1080 && VideoDownloader.resolvedFps==24,"HLS fallback metadata lost across client attempts");
        System.out.println(checks + " YouTube session checks passed");
    }
}
