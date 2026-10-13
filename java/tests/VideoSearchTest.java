package org.bstheater.quest;

import com.grack.nanojson.*;
import java.io.IOException;

/** Offline validation plus an optional public, cookie-free extractor smoke test. */
public final class VideoSearchTest {
    private static int checks;
    private static void check(boolean value, String message) {
        checks++; if(!value)throw new AssertionError(message);
    }
    private static void rejects(String query) throws Exception {
        try{VideoDownloader.search(query);throw new AssertionError("Invalid query accepted");}
        catch(IOException expected){checks++;}
    }
    public static void main(String[] args) throws Exception {
        VideoDownloader.setCookieFile(null);
        rejects(null);rejects("");rejects("   ");rejects("x".repeat(513));
        rejects("https://example.org/video");
        rejects("https://youtube.com@evil.example/video");
        if(args.length>0&&args[0].equals("--online")) {
            JsonArray items=JsonParser.array().from(VideoDownloader.search("TAK PPPP"));
            check(!items.isEmpty()&&items.size()<=20,"Search has no bounded results");
            for(Object entry:items) {
                JsonObject item=(JsonObject)entry;
                check(VideoDownloader.isYoutube(item.getString("url")),"Search returned a non-YouTube URL");
                check(!item.getString("title").isEmpty(),"Missing result title");
                check(item.getInt("duration")>=0&&item.getInt("duration")<=86400,"Invalid result duration");
                check(item.getString("thumbnail").isEmpty()||item.getString("thumbnail").startsWith("https://"),"Thumbnail is not HTTPS");
            }
            System.out.println("Public cookie-free YouTube search returned "+items.size()+" results");
        }
        System.out.println(checks+" YouTube search checks passed");
    }
}
