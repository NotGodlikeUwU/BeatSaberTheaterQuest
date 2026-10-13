import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import java.nio.ByteBuffer;

/** Device-only codec check; does not claim Unity playback or song synchronization. */
public final class AndroidVideoCheck {
    public static void main(String[] args) throws Exception {
        MediaExtractor extractor = new MediaExtractor();
        MediaCodec codec = null;
        try {
            extractor.setDataSource(args[0]);
            MediaFormat format = null;
            for (int i=0;i<extractor.getTrackCount();i++) {
                MediaFormat track=extractor.getTrackFormat(i);
                if ("video/avc".equals(track.getString(MediaFormat.KEY_MIME))) { format=track;extractor.selectTrack(i);break; }
            }
            if (format==null) throw new IllegalStateException("Downloaded MP4 has no H.264 track");
            codec=MediaCodec.createDecoderByType("video/avc");
            codec.configure(format,null,null,0);
            codec.start();
            MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();
            boolean ended=false,decoded=false;
            long deadline=System.nanoTime()+30_000_000_000L;
            while (!decoded && System.nanoTime()<deadline) {
                if (!ended) {
                    int index=codec.dequeueInputBuffer(10000);
                    if (index>=0) {
                        ByteBuffer buffer=codec.getInputBuffer(index);
                        int size=extractor.readSampleData(buffer,0);
                        if (size<0) { codec.queueInputBuffer(index,0,0,0,MediaCodec.BUFFER_FLAG_END_OF_STREAM);ended=true; }
                        else { codec.queueInputBuffer(index,0,size,extractor.getSampleTime(),0);extractor.advance(); }
                    }
                }
                int index=codec.dequeueOutputBuffer(info,10000);
                if (index>=0) {
                    decoded=info.size>0;
                    codec.releaseOutputBuffer(index,false);
                    if (!decoded && (info.flags&MediaCodec.BUFFER_FLAG_END_OF_STREAM)!=0) break;
                }
            }
            if (!decoded) throw new IllegalStateException("Android decoder produced no video frame");
            System.out.println("Quest Android H.264 frame decoded: "+format.getInteger(MediaFormat.KEY_WIDTH)+"x"+format.getInteger(MediaFormat.KEY_HEIGHT)
                +", duration "+format.getLong(MediaFormat.KEY_DURATION)/1000+" ms");
            extractor.seekTo(0,MediaExtractor.SEEK_TO_CLOSEST_SYNC);
            long first=Long.MAX_VALUE,last=Long.MIN_VALUE;int samples=0;
            for(;samples<240 && extractor.getSampleTime()>=0;samples++) {
                long time=extractor.getSampleTime();first=Math.min(first,time);last=Math.max(last,time);extractor.advance();
            }
            if(samples>1 && last>first)System.out.println("MP4 video frame rate: "+String.format(java.util.Locale.ROOT,"%.2f",(samples-1)*1000000.0/(last-first))+" fps");
        } finally {
            if (codec!=null) {codec.stop();codec.release();}
            extractor.release();
            // app_process has no Activity lifetime to keep codec teardown threads alive.
            Thread.sleep(500);
        }
    }
}
