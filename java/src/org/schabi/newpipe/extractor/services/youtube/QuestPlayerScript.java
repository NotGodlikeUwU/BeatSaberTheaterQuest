package org.schabi.newpipe.extractor.services.youtube;
/** Exposes the extractor's existing YouTube-only public player-script fetch for the EJS fallback. */
public final class QuestPlayerScript {
    public static String fetch(String videoId) throws org.schabi.newpipe.extractor.exceptions.ParsingException {
        return YoutubeJavaScriptExtractor.extractJavaScriptPlayerCode(videoId);
    }
}
