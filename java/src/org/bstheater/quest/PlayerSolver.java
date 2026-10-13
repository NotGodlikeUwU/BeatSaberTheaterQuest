package org.bstheater.quest;

import com.grack.nanojson.*;
import java.io.IOException;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import org.schabi.newpipe.extractor.services.youtube.QuestPlayerScript;

/** Official yt-dlp EJS protocol, executed locally by the embedded QuickJS engine. */
final class PlayerSolver {
    private static String prepared;
    private static long preparedAt;
    private static final Map<String,String> cache = new LinkedHashMap<>();
    static synchronized String solve(String videoId, String type, String challenge) throws IOException {
        VideoDownloader.checkCancelled();
        if (System.nanoTime()-preparedAt > 3_600_000_000_000L) { prepared=null; cache.clear(); }
        String key = type + ":" + challenge;
        if (cache.containsKey(key)) return cache.get(key);
        try {
            JsonBuilder<JsonObject> builder = JsonObject.builder().value("type",prepared==null ? "player" : "preprocessed");
            if (prepared==null) builder.value("player",QuestPlayerScript.fetch(videoId)).value("output_preprocessed",true);
            else builder.value("preprocessed_player",prepared);
            builder.array("requests").object().value("type",type).array("challenges").value(challenge).end().end().end();
            JsonObject result = JsonParser.object().from(NativePlayerSolver.evaluate(JsonWriter.string(builder.done())));
            JsonArray responses = result.getArray("responses");
            if (!result.getString("type","").equals("result") || responses.isEmpty()) throw new IOException("YouTube EJS decoder returned no result");
            JsonObject response = (JsonObject) responses.get(0);
            if (!response.getString("type","").equals("result")) throw new IOException("YouTube EJS could not decode " + type);
            String value = response.getObject("data").getString(challenge);
            if (value==null || value.isEmpty() || value.contains("_w8_")) throw new IOException("YouTube EJS returned an invalid decoder result");
            if (result.has("preprocessed_player")) { prepared=result.getString("preprocessed_player"); preparedAt=System.nanoTime(); }
            if (cache.size()>=128) cache.clear();
            cache.put(key,value);
            VideoDownloader.checkCancelled();
            return value;
        } catch (IOException error) { throw error; }
        catch (Exception error) { throw new IOException("YouTube EJS decoding failed",error); }
    }
    static String decodeN(String videoId, String source) throws IOException {
        URL url = new URL(source);
        List<String> parts = new ArrayList<>();
        for (String part : url.getQuery().split("&")) {
            String[] pair = part.split("=",2);
            if (pair.length==2 && pair[0].equals("n")) {
                String value = solve(videoId,"n",URLDecoder.decode(pair[1],StandardCharsets.UTF_8));
                part="n="+URLEncoder.encode(value,StandardCharsets.UTF_8);
            }
            parts.add(part);
        }
        return url.getProtocol()+"://"+url.getAuthority()+url.getPath()+"?"+String.join("&",parts);
    }
    static String decodeManifest(String videoId,String source) throws IOException {
        URL url=new URL(source);String path=url.getPath();
        java.util.regex.Matcher matcher=java.util.regex.Pattern.compile("/n/([^/]+)").matcher(path);
        if(!matcher.find())return source;
        String decoded=solve(videoId,"n",URLDecoder.decode(matcher.group(1),StandardCharsets.UTF_8));
        path=path.substring(0,matcher.start(1))+URLEncoder.encode(decoded,StandardCharsets.UTF_8)+path.substring(matcher.end(1));
        return url.getProtocol()+"://"+url.getAuthority()+path+(url.getQuery()==null?"":"?"+url.getQuery());
    }
}
