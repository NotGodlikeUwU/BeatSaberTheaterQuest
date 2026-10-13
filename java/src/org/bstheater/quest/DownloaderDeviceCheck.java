package org.bstheater.quest;
/** Runs the same downloader DEX on a connected Quest, without logging signed URLs or cookie values. */
public final class DownloaderDeviceCheck {
    public static void main(String[] args) {
        if (args.length != 4) { System.err.println("Expected video URL, cookie file, output MP4 and solver library paths"); System.exit(2); }
        try {
            System.setProperty("bstheater.solver.library",args[3]);
            System.setProperty("bstheater.keep.transport","true");
            VideoDownloader.setCookieFile(args[1]);
            VideoDownloader.prepare();
            VideoDownloader.download(args[0], args[2], 1080, 900, 512L*1024*1024);
            System.out.println(VideoDownloader.sessionStatus);
            System.out.println("Quest MP4 download and validation passed: " + VideoDownloader.downloadedBytes + " bytes");
            System.out.println("Selected format: "+VideoDownloader.resolvedHeight+"p / "+VideoDownloader.resolvedFps+" fps"); for (String diagnostic: AuthenticatedYoutube.diagnostics) System.out.println(diagnostic);
        } catch (Exception error) {
            // Only the sanitized top-level message, never a response body, credential or signed media URL.
            System.err.println(VideoDownloader.sessionStatus);
            System.err.println("Quest download failed: " + error.getMessage());
            for (String diagnostic: AuthenticatedYoutube.diagnostics) System.err.println(diagnostic);
            for (Throwable cause=error.getCause(); cause!=null; cause=cause.getCause()) {
                String location=cause.getStackTrace().length==0 ? "unknown" : cause.getStackTrace()[0].getClassName()+"."+cause.getStackTrace()[0].getMethodName();
                System.err.println("Diagnostic: " + cause.getClass().getSimpleName() + " at " + location);
            }
            System.exit(1);
        }
    }
}

