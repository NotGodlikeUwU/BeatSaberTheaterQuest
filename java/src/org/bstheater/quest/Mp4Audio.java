package org.bstheater.quest;
import android.media.*;
import java.io.*;
import java.nio.ByteBuffer;
import java.util.*;

/** Keeps H.264/AAC samples unchanged; full-quality video is never re-encoded. */
final class Mp4Audio {
    static boolean hasAudio(File file) throws IOException {
        MediaExtractor extractor=new MediaExtractor();
        try {extractor.setDataSource(file.toString());for(int i=0;i<extractor.getTrackCount();i++)
            if(extractor.getTrackFormat(i).getString(MediaFormat.KEY_MIME).startsWith("audio/"))return true;
            return false;
        }finally{extractor.release();}
    }
    static void remux(File input,File output,long limit) throws IOException {mux(input,null,output,limit);}
    static void mux(File video,File audio,File output,long limit) throws IOException {
        List<MediaExtractor> extractors=new ArrayList<>();List<int[]> tracks=new ArrayList<>();
        MediaMuxer muxer=null;boolean started=false;
        try {
            muxer=new MediaMuxer(output.toString(),MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            for(File file:audio==null?Collections.singletonList(video):Arrays.asList(video,audio)) {
                MediaExtractor extractor=new MediaExtractor();extractors.add(extractor);extractor.setDataSource(file.toString());
                for(int i=0;i<extractor.getTrackCount();i++) {
                    MediaFormat format=extractor.getTrackFormat(i);String mime=format.getString(MediaFormat.KEY_MIME);
                    if(!"video/avc".equals(mime)&&!"audio/mp4a-latm".equals(mime))continue;
                    if(file.equals(audio)&&!mime.startsWith("audio/"))continue;
                    tracks.add(new int[]{extractors.size()-1,i,muxer.addTrack(format)});
                }
            }
            if(tracks.isEmpty())throw new IOException("Stream contains no H.264 video or AAC audio");
            muxer.start();started=true;ByteBuffer buffer=ByteBuffer.allocateDirect(16*1024*1024);MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();
            // Keep a shared origin for tracks demuxed from the same transport.
            long[] origins=new long[extractors.size()];Arrays.fill(origins,Long.MAX_VALUE);
            for(int[] track:tracks) {
                MediaExtractor extractor=extractors.get(track[0]);extractor.selectTrack(track[1]);extractor.seekTo(0,MediaExtractor.SEEK_TO_CLOSEST_SYNC);
                if(extractor.getSampleTime()>=0)origins[track[0]]=Math.min(origins[track[0]],extractor.getSampleTime());
                extractor.unselectTrack(track[1]);
            }
            for(int[] track:tracks) {
                MediaExtractor extractor=extractors.get(track[0]);extractor.selectTrack(track[1]);extractor.seekTo(0,MediaExtractor.SEEK_TO_CLOSEST_SYNC);
                int samples=0;
                while(true) {
                    VideoDownloader.checkCancelled();buffer.clear();int size=extractor.readSampleData(buffer,0);if(size<0)break;
                    if(size>buffer.capacity())throw new IOException("MP4 sample exceeds supported size");
                    // B-frame presentation timestamps are allowed to go backwards in decode order.
                    long time=Math.max(0,extractor.getSampleTime()-origins[track[0]]);
                    info.set(0,size,time,extractor.getSampleFlags()&MediaCodec.BUFFER_FLAG_KEY_FRAME);
                    muxer.writeSampleData(track[2],buffer,info);samples++;
                    if(output.length()>limit)throw new IOException("Video with audio exceeds configured size limit");
                    extractor.advance();
                }
                if(samples==0)throw new IOException("Empty MP4 track");extractor.unselectTrack(track[1]);
            }
            muxer.stop();started=false;
            if(output.length()>limit)throw new IOException("Completed MP4 exceeds configured size limit");
        }catch(RuntimeException error){throw new IOException("Android MP4 remux failed",error);}
        finally {for(MediaExtractor extractor:extractors)extractor.release();if(muxer!=null){if(started)try{muxer.stop();}catch(RuntimeException ignored){}muxer.release();}}
    }
}
