package org.bstheater.quest;

import java.io.*;
import java.net.URL;
import java.net.HttpCookie;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.*;

/** Explicit local Netscape-cookie import; never reads another app's private storage. */
final class YoutubeCookies {
    private static final int LIMIT = 256 * 1024;
    private final List<Cookie> cookies = new ArrayList<>();
    private static final class Cookie {
        String domain, path, name, value;
        boolean subdomains;
        long expires;
        boolean matches(URL url, long now) {
            String host = url.getHost().toLowerCase(Locale.ROOT);
            String requestPath = url.getPath().isEmpty() ? "/" : url.getPath();
            return url.getProtocol().equals("https") && youtubeHost(host)
                && (expires == 0 || expires > now)
                && (host.equals(domain) || (subdomains && host.endsWith("." + domain)))
                && requestPath.startsWith(path)
                && (requestPath.length() == path.length() || path.endsWith("/") || requestPath.charAt(path.length()) == '/');
        }
    }
    static boolean youtubeHost(String host) {
        return host.equals("youtube.com") || host.endsWith(".youtube.com");
    }
    static YoutubeCookies read(File file, long now) throws IOException {
        if (file.length() > LIMIT) throw new IOException("youtube-cookies.txt exceeds 256 KiB");
        byte[] bytes;
        try (InputStream input = Files.newInputStream(file.toPath())) {
            bytes = input.readNBytes(LIMIT + 1);
        }
        if (bytes.length > LIMIT) throw new IOException("youtube-cookies.txt exceeds 256 KiB");
        return parse(new String(bytes, StandardCharsets.UTF_8), now);
    }
    static YoutubeCookies parse(String text, long now) throws IOException {
        if (text.startsWith("\uFEFF")) text = text.substring(1);
        if (!text.startsWith("# Netscape HTTP Cookie File") && !text.startsWith("# HTTP Cookie File"))
            throw new IOException("youtube-cookies.txt must be a Netscape cookie export");
        YoutubeCookies result = new YoutubeCookies();
        int lineNumber = 0;
        for (String line : text.split("\r?\n")) {
            lineNumber++;
            if (line.startsWith("#HttpOnly_")) line = line.substring(10);
            else if (line.startsWith("#") || line.trim().isEmpty()) continue;
            String[] fields = line.split("\t", -1);
            if (fields.length != 7) throw malformed(lineNumber);
            String domain = fields[0].toLowerCase(Locale.ROOT);
            if (domain.startsWith(".")) domain = domain.substring(1);
            if (!youtubeHost(domain)) continue;
            if (!fields[1].matches("TRUE|FALSE") || !fields[3].matches("TRUE|FALSE")
                || !fields[2].startsWith("/") || !fields[5].matches("[!#$%&'*+.^_`|~0-9A-Za-z-]+")
                || !fields[6].matches("[\u0021-\u003A\u003C-\u007E]*")) throw malformed(lineNumber);
            Cookie cookie = new Cookie();
            cookie.domain = domain; cookie.subdomains = fields[1].equals("TRUE");
            cookie.path = fields[2]; cookie.name = fields[5]; cookie.value = fields[6];
            try { cookie.expires = Long.parseLong(fields[4]); }
            catch (NumberFormatException error) { throw malformed(lineNumber); }
            if (cookie.expires < 0) throw malformed(lineNumber);
            if (cookie.expires != 0 && cookie.expires <= now) continue;
            result.cookies.add(cookie);
            if (result.cookies.size() > 1024) throw new IOException("Too many YouTube cookies");
        }
        return result;
    }
    private static IOException malformed(int line) {
        // Never include a cookie line/value in errors or logs.
        return new IOException("Invalid YouTube cookie export at line " + line);
    }
    private Map<String, String> values(URL url, long now) {
        Map<String, String> values = new LinkedHashMap<>();
        List<Cookie> matching = new ArrayList<>();
        for (Cookie cookie : cookies) if (cookie.matches(url, now)) matching.add(cookie);
        matching.sort(Comparator.comparingInt((Cookie cookie) -> cookie.path.length()).reversed());
        for (Cookie cookie : matching) values.putIfAbsent(cookie.name, cookie.value);
        return values;
    }
    boolean authenticated(long now) throws IOException {
        Map<String, String> values = values(new URL("https://www.youtube.com/youtubei/v1/player"), now);
        return values.containsKey("LOGIN_INFO") && (!sid(values, "SAPISID").isEmpty()
            || !sid(values, "__Secure-1PAPISID").isEmpty() || !sid(values, "__Secure-3PAPISID").isEmpty());
    }
    private static String sid(Map<String, String> values, String name) { return values.getOrDefault(name, ""); }
    void receive(URL source, Map<String,List<String>> headers, long now, String file) throws IOException {
        if (!source.getProtocol().equals("https") || !youtubeHost(source.getHost().toLowerCase(Locale.ROOT))) return;
        boolean changed=false;
        for (Map.Entry<String,List<String>> header:headers.entrySet()) {
            if (header.getKey()==null || !header.getKey().equalsIgnoreCase("Set-Cookie")) continue;
            for (String line:header.getValue()) {
                if (line.length()>16384) continue;
                List<HttpCookie> incoming;
                try { incoming=HttpCookie.parse(line); } catch (IllegalArgumentException ignored) { continue; }
                for (HttpCookie item:incoming) {
                    String domain=item.getDomain()==null ? source.getHost() : item.getDomain();
                    domain=domain.toLowerCase(Locale.ROOT);
                    if(domain.startsWith(".")) domain=domain.substring(1);
                    if (!youtubeHost(domain) || !(source.getHost().equalsIgnoreCase(domain) || source.getHost().toLowerCase(Locale.ROOT).endsWith("."+domain))) continue;
                    if (!item.getName().matches("[!#$%&'*+.^_`|~0-9A-Za-z-]+") || !item.getValue().matches("[\u0021-\u003A\u003C-\u007E]*")) continue;
                    String path=item.getPath()==null ? "/" : item.getPath();
                    if (!path.startsWith("/") || path.contains("\t") || path.contains("\n") || path.contains("\r")) continue;
                    final String matchDomain=domain;
                    cookies.removeIf(c -> c.domain.equals(matchDomain) && c.path.equals(path) && c.name.equals(item.getName()));
                    changed=true;
                    if (item.getMaxAge()==0) continue;
                    Cookie cookie=new Cookie();cookie.domain=domain;cookie.subdomains=item.getDomain()!=null;
                    cookie.path=path;cookie.name=item.getName();cookie.value=item.getValue();
                    cookie.expires=item.getMaxAge()<0 ? 0 : now+Math.min(item.getMaxAge(),400L*24*3600);
                    if(cookies.size()>=1024)throw new IOException("YouTube cookie refresh exceeds limits");
                    cookies.add(cookie);
                }
            }
        }
        if (changed && file!=null) {
            StringBuilder text=new StringBuilder("# Netscape HTTP Cookie File\n");
            for (Cookie cookie:cookies) if (cookie.expires==0 || cookie.expires>now) text.append("#HttpOnly_").append(cookie.subdomains ? "." : "")
                .append(cookie.domain).append('\t').append(cookie.subdomains ? "TRUE" : "FALSE").append('\t').append(cookie.path)
                .append("\tTRUE\t").append(cookie.expires).append('\t').append(cookie.name).append('\t').append(cookie.value).append('\n');
            if (text.length()>LIMIT || cookies.size()>1024) throw new IOException("YouTube cookie refresh exceeds limits");
            File destination=new File(file),temporary=new File(file+".refresh");
            try (FileOutputStream out=new FileOutputStream(temporary)) { out.write(text.toString().getBytes(StandardCharsets.UTF_8)); out.getFD().sync(); }
            try { java.nio.file.Files.move(temporary.toPath(),destination.toPath(),java.nio.file.StandardCopyOption.ATOMIC_MOVE,java.nio.file.StandardCopyOption.REPLACE_EXISTING); }
            catch (java.nio.file.AtomicMoveNotSupportedException error) { java.nio.file.Files.move(temporary.toPath(),destination.toPath(),java.nio.file.StandardCopyOption.REPLACE_EXISTING); }
        }
    }
    Map<String, List<String>> headers(URL url, long now) throws IOException {
        Map<String, List<String>> headers = new LinkedHashMap<>();
        Map<String, String> values = values(url, now);
        if (values.isEmpty()) return headers;
        List<String> pairs = new ArrayList<>();
        values.forEach((name, value) -> pairs.add(name + "=" + value));
        headers.put("Cookie", Collections.singletonList(String.join("; ", pairs)));
        if (!url.getPath().startsWith("/youtubei/v1/")) return headers;
        String thirdParty = sid(values, "__Secure-3PAPISID");
        String sapisid = sid(values, "SAPISID");
        if (sapisid.isEmpty()) sapisid = thirdParty;
        List<String> auth = new ArrayList<>();
        addHash(auth, "SAPISIDHASH", sapisid, now);
        addHash(auth, "SAPISID1PHASH", sid(values, "__Secure-1PAPISID"), now);
        addHash(auth, "SAPISID3PHASH", thirdParty, now);
        if (!auth.isEmpty()) {
            headers.put("Authorization", Collections.singletonList(String.join(" ", auth)));
            headers.put("Origin", Collections.singletonList("https://www.youtube.com"));
            headers.put("X-Origin", Collections.singletonList("https://www.youtube.com"));
            headers.put("X-Goog-AuthUser", Collections.singletonList("0"));
        }
        return headers;
    }
    private static void addHash(List<String> auth, String scheme, String sid, long now) throws IOException {
        if (sid.isEmpty()) return;
        try {
            byte[] digest = MessageDigest.getInstance("SHA-1").digest(
                (now + " " + sid + " https://www.youtube.com").getBytes(StandardCharsets.UTF_8));
            StringBuilder hex = new StringBuilder();
            for (byte value : digest) hex.append(String.format(Locale.ROOT, "%02x", value & 255));
            auth.add(scheme + " " + now + "_" + hex);
        } catch (NoSuchAlgorithmException error) { throw new IOException("SHA-1 unavailable", error); }
    }
}
