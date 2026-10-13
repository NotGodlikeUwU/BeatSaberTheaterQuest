package org.bstheater.quest;
import java.io.IOException;
import java.util.*;
public final class HlsManifestTest {
    static int checks;
    static void check(boolean condition,String message){checks++;if(!condition)throw new AssertionError(message);}
    public static void main(String[] args)throws Exception {
        String manifest="#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1000,RESOLUTION=1920x1080,FRAME-RATE=30,CODECS=\"avc1.640028,mp4a.40.2\"\n30.m3u8\n#EXT-X-STREAM-INF:BANDWIDTH=2000,RESOLUTION=1920x1080,FRAME-RATE=60,CODECS=\"avc1.64002a,mp4a.40.2\"\n60.m3u8\n#EXT-X-STREAM-INF:RESOLUTION=3840x2160,CODECS=\"avc1\"\n4k.m3u8\n#EXT-X-STREAM-INF:RESOLUTION=1280x720,CODECS=\"vp09\"\nvp9.m3u8\n";
        List<HlsVideo.Rendition> formats=HlsVideo.renditions("https://example.org/hls/master.m3u8",manifest,1080);
        check(formats.size()==2,"Codec/height filtering failed");
        check(formats.get(0).fps==60 && formats.get(0).url.equals("https://example.org/hls/60.m3u8"),"1080p60 preference/relative URL failed");
        check(HlsVideo.renditions("https://example.org/index.m3u8",manifest,720).isEmpty(),"Resolution cap ignored");
        String audio="#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",URI=\"audio/index.m3u8\"\n";
        String withAudio=manifest.replace("#EXTM3U\n","#EXTM3U\n"+audio).replace("CODECS=","AUDIO=\"aac\",CODECS=");
        check(HlsVideo.renditions("https://example.org/index.m3u8",withAudio,1080).get(0).audioUrl.equals("https://example.org/audio/index.m3u8"),"HLS AAC group missing");
        boolean audioDenied=false;try{HlsVideo.renditions("https://example.org/index.m3u8",withAudio.replace("audio/index.m3u8","http://other.example/audio.m3u8"),1080);}catch(IOException error){audioDenied=true;}
        check(audioDenied,"Insecure HLS audio accepted");
        String playlist="#EXTM3U\n#EXTINF:3,\na.ts\n#EXTINF:3,\nb.ts\n#EXT-X-ENDLIST\n";
        check(HlsVideo.segments("https://example.org/hls/index.m3u8",playlist).size()==2,"Segments lost");
        boolean denied=false;try{HlsVideo.segments("https://example.org/",playlist.replace("a.ts","http://example.org/a.ts"));}catch(IOException error){denied=true;}
        check(denied,"Insecure segment accepted");
        denied=false;try{HlsVideo.segments("https://example.org/",playlist.replace("#EXTINF:3,","#EXT-X-KEY:METHOD=AES-128,URI=\"key\""));}catch(IOException error){denied=true;}
        check(denied,"Encrypted stream accepted");
        denied=false;try{HlsVideo.segments("https://example.org/",playlist.replace("#EXT-X-ENDLIST",""));}catch(IOException error){denied=true;}
        check(denied,"Live stream accepted");
        System.out.println(checks+" HLS manifest checks passed");
    }
}
