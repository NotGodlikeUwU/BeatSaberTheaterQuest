package org.bstheater.quest;

import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.media.MediaCodec;
import java.io.*;
import java.net.URL;
import java.nio.ByteBuffer;
import java.util.*;
import java.util.regex.*;
import java.util.concurrent.*;
import org.schabi.newpipe.extractor.downloader.Downloader;

/** Downloads YouTube's Safari HLS rendition and remuxes video without re-encoding. */
final class HlsVideo {
    static final class Rendition {
        final String url; final int height, fps;
        Rendition(String url,int height,int fps){this.url=url;this.height=height;this.fps=fps;}
    }
    static Map<String,String> attributes(String text) {
        Map<String,String> result=new HashMap<>();
        Matcher matcher=Pattern.compile("([A-Z0-9-]+)=(\"[^\"]*\"|[^,]*)").matcher(text);
        while(matcher.find()) result.put(matcher.group(1),matcher.group(2).replace("\"",""));
        return result;
    }
    static List<Rendition> renditions(String base,String text,int maximum) throws IOException {
        if(!text.startsWith("#EXTM3U"))throw new IOException("Invalid YouTube HLS manifest");
        List<Rendition> result=new ArrayList<>();Map<String,String> pending=null;
        for(String line:text.split("\\r?\\n")) {
            line=line.trim();
            if(line.startsWith("#EXT-X-STREAM-INF:"))pending=attributes(line.substring(18));
            else if(!line.isEmpty() && !line.startsWith("#") && pending!=null) {
                String[] dimensions=pending.getOrDefault("RESOLUTION","0x0").split("x");
                try {
                    int height=Integer.parseInt(dimensions[1]);int fps=(int)Math.round(Double.parseDouble(pending.getOrDefault("FRAME-RATE","30")));
                    if(height>0 && height<=maximum && pending.getOrDefault("CODECS","").contains("avc1"))
                        result.add(new Rendition(VideoDownloader.checkedUrl(new URL(new URL(base),line).toString()).toString(),height,fps));
                }catch(NumberFormatException|IndexOutOfBoundsException ignored){}
                pending=null;
            }
        }
        result.sort(Comparator.comparingInt((Rendition r)->r.height).thenComparingInt(r->r.fps).reversed());
        return result;
    }
    static Rendition choose(Downloader downloader,String manifest,int maximum,AuthenticatedYoutube.MediaVerifier verifier) throws IOException {
        String text=fetch(downloader,manifest);IOException last=null;
        for(Rendition rendition:renditions(manifest,text,maximum))try {
            String playlist=fetch(downloader,rendition.url);
            List<String> segments=segments(rendition.url,playlist);
            if(segments.isEmpty())continue;verifier.verify(segments.get(0));return rendition;
        }catch(IOException error){last=error;}
        throw last==null?new IOException("YouTube HLS has no compatible video rendition"):last;
    }
    static String fetch(Downloader downloader,String url) throws IOException {
        try {return downloader.get(url).responseBody();}
        catch(org.schabi.newpipe.extractor.exceptions.ReCaptchaException error){throw new IOException("YouTube requires browser verification",error);}
    }
    static List<String> segments(String base,String text) throws IOException {
        if(!text.startsWith("#EXTM3U") || !text.contains("#EXT-X-ENDLIST"))throw new IOException("Only complete HLS videos are supported");
        List<String> result=new ArrayList<>();
        for(String line:text.split("\\r?\\n")) {
            line=line.trim();
            if(line.startsWith("#EXT-X-KEY:") && !line.contains("METHOD=NONE"))throw new IOException("Encrypted HLS video is unsupported");
            if(line.startsWith("#EXT-X-BYTERANGE:"))throw new IOException("HLS byte-range segments are unsupported");
            if(line.startsWith("#EXT-X-MAP:")) {
                String map=attributes(line.substring(11)).get("URI");if(map!=null)result.add(new URL(new URL(base),map).toString());
            }else if(!line.isEmpty() && !line.startsWith("#"))result.add(new URL(new URL(base),line).toString());
            if(result.size()>4096)throw new IOException("HLS video has too many segments");
        }
        for(String url:result)VideoDownloader.checkedUrl(url);
        return result;
    }
    static void download(String playlist,File output,long limit) throws IOException {
        String text=VideoDownloader.fetchText(playlist);
        List<String> urls=segments(playlist,text);
        if(urls.isEmpty())throw new IOException("Empty HLS playlist");
        File transport=new File(output+".ts");
        ExecutorService workers=Executors.newFixedThreadPool(3);
        Map<Integer,Future<File>> pending=new HashMap<>();
        List<File> parts=new ArrayList<>();
        try {
            try(FileOutputStream stream=new FileOutputStream(transport)) {
                for(int index=0;index<urls.size();index++) {
                    while(pending.size()<3 && index+pending.size()<urls.size()) {
                        int task=index+pending.size();String url=urls.get(task);
                        File part=new File(output+".segment-"+task);parts.add(part);
                        pending.put(task,workers.submit(()->{try(FileOutputStream segment=new FileOutputStream(part)){VideoDownloader.copyMedia(url,segment,limit);}return part;}));
                    }
                    VideoDownloader.checkCancelled();
                    VideoDownloader.phase="Downloading HD video ("+(index+1)+"/"+urls.size()+")";
                    File part;
                    try {part=pending.remove(index).get();}
                    catch(InterruptedException error){Thread.currentThread().interrupt();throw new InterruptedIOException("HD download cancelled");}
                    catch(ExecutionException error){if(error.getCause() instanceof IOException)throw (IOException)error.getCause();throw new IOException("HLS segment download failed",error.getCause());}
                    try(InputStream segment=new FileInputStream(part)){segment.transferTo(stream);}part.delete();
                }
                stream.getFD().sync();
            }
            VideoDownloader.phase="Saving HD MP4";
            remux(transport,output,limit);
        }finally{
            for(Future<File> future:pending.values())future.cancel(true);workers.shutdownNow();
            try{workers.awaitTermination(20,TimeUnit.SECONDS);}catch(InterruptedException error){Thread.currentThread().interrupt();}
            for(File part:parts)if(part.exists())part.delete();
            if(!Boolean.getBoolean("bstheater.keep.transport") && transport.exists())transport.delete();
        }
    }
    static int[] dimensions(File video) throws IOException {
        MediaExtractor extractor=new MediaExtractor();
        try {
            extractor.setDataSource(video.toString());
            for(int i=0;i<extractor.getTrackCount();i++) {
                MediaFormat format=extractor.getTrackFormat(i);
                if(format.getString(MediaFormat.KEY_MIME).startsWith("video/"))return new int[]{format.getInteger(MediaFormat.KEY_HEIGHT),format.containsKey(MediaFormat.KEY_FRAME_RATE)?format.getInteger(MediaFormat.KEY_FRAME_RATE):0};
            }
            return new int[]{0,0};
        }finally{extractor.release();}
    }
    static void remux(File input,File output,long limit) throws IOException {
        MediaExtractor extractor=new MediaExtractor();MediaMuxer muxer=null;boolean started=false;
        try {
            extractor.setDataSource(input.toString());
            int track=-1;MediaFormat format=null;
            for(int i=0;i<extractor.getTrackCount();i++) {
                MediaFormat candidate=extractor.getTrackFormat(i);
                if("video/avc".equals(candidate.getString(MediaFormat.KEY_MIME))){track=i;format=candidate;break;}
            }
            if(track<0)throw new IOException("HLS stream contains no H.264 video");
            extractor.selectTrack(track);
            muxer=new MediaMuxer(output.toString(),MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            int target=muxer.addTrack(format);muxer.start();started=true;
            int capacity=format.containsKey(MediaFormat.KEY_MAX_INPUT_SIZE)?format.getInteger(MediaFormat.KEY_MAX_INPUT_SIZE):4*1024*1024;
            ByteBuffer buffer=ByteBuffer.allocateDirect(Math.max(4*1024*1024,Math.min(16*1024*1024,capacity)));
            MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();long first=-1,last=-1;int count=0;
            while(true) {
                VideoDownloader.checkCancelled();buffer.clear();int size=extractor.readSampleData(buffer,0);if(size<0)break;
                long time=extractor.getSampleTime();if(first<0)first=time;
                time-=first;
                // H.264 B-frames are stored in decode order; their presentation timestamps may go backwards.
                if(time<0)time=0;
                info.set(0,size,time,extractor.getSampleFlags()&MediaCodec.BUFFER_FLAG_KEY_FRAME);
                muxer.writeSampleData(target,buffer,info);last=time;count++;
                if(output.length()>limit)throw new IOException("HD video exceeds configured size limit");
                extractor.advance();
            }
            if(count==0)throw new IOException("HLS video has no decoded samples");
            muxer.stop();started=false;
        }catch(RuntimeException error){throw new IOException("Android could not convert HLS video to MP4",error);}
        finally{extractor.release();if(muxer!=null){if(started)try{muxer.stop();}catch(RuntimeException ignored){}muxer.release();}}
    }
}
