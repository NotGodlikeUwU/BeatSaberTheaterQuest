package org.bstheater.quest;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;

/** Minimal loopback CDP server. Only synthetic test cookies are served. */
public final class BrowserCookieFixture {
    static String line(InputStream input) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream(); int b;
        while ((b=input.read())!=-1 && b!='\n') { if (b!='\r') out.write(b); if(out.size()>16384) throw new IOException("Oversized test header"); }
        return out.toString(StandardCharsets.US_ASCII);
    }
    public static void main(String[] args) throws Exception {
        try(ServerSocket server=new ServerSocket(0,1,InetAddress.getByName("127.0.0.1"))) {
            server.setSoTimeout(15000);
            Files.writeString(Paths.get(args[0]),Integer.toString(server.getLocalPort()));
            try(Socket http=server.accept()) {
                String first=line(http.getInputStream());
                if(!first.startsWith("GET /json/list")) throw new IOException("Wrong discovery request");
                while(!line(http.getInputStream()).isEmpty()) { }
                String body="[{\"type\":\"page\",\"url\":\"https://www.youtube.com/watch?v=abcdefghijk\",\"webSocketDebuggerUrl\":\"ws://127.0.0.1:"+server.getLocalPort()+"/devtools/page/fixture\"}]";
                byte[] bytes=body.getBytes(StandardCharsets.UTF_8);
                http.getOutputStream().write(("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "+bytes.length+"\r\nConnection: close\r\n\r\n").getBytes(StandardCharsets.US_ASCII));
                http.getOutputStream().write(bytes); http.getOutputStream().flush();
            }
            try(Socket websocket=server.accept()) {
                websocket.setSoTimeout(15000);
                InputStream input=websocket.getInputStream(); OutputStream output=websocket.getOutputStream();
                String first=line(input), key=null, header;
                if(!first.startsWith("GET /devtools/page/fixture")) throw new IOException("Wrong WebSocket target");
                while(!(header=line(input)).isEmpty()) if(header.toLowerCase(Locale.ROOT).startsWith("sec-websocket-key:")) key=header.substring(header.indexOf(':')+1).trim();
                if(key==null) throw new IOException("Missing WebSocket key");
                String accept=Base64.getEncoder().encodeToString(MessageDigest.getInstance("SHA-1").digest((key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11").getBytes(StandardCharsets.US_ASCII)));
                output.write(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "+accept+"\r\n\r\n").getBytes(StandardCharsets.US_ASCII)); output.flush();
                input.read(); int lengthByte=input.read(); boolean masked=(lengthByte&128)!=0; int length=lengthByte&127;
                if(length==126) length=(input.read()<<8)|input.read();
                if(length>16384 || !masked) throw new IOException("Invalid client frame");
                byte[] mask=input.readNBytes(4), request=input.readNBytes(length);
                for(int i=0;i<request.length;i++) request[i]^=mask[i%4];
                String command=new String(request,StandardCharsets.UTF_8);
                if(!command.contains("Network.getCookies") || !command.contains("\"urls\":[\"https://www.youtube.com/\"]")) throw new IOException("Cookie request not scoped to YouTube");
                String common=",\"path\":\"/\",\"secure\":true,\"httpOnly\":true,\"session\":true,\"expires\":-1}";
                String payload="{\"id\":1,\"result\":{\"cookies\":["
                    +"{\"domain\":\".youtube.com\",\"name\":\"LOGIN_INFO\",\"value\":\"synthetic-login\""+common+","
                    +"{\"domain\":\".youtube.com\",\"name\":\"SAPISID\",\"value\":\"synthetic-sid\""+common+","
                    +"{\"domain\":\".google.com\",\"name\":\"OTHER_ACCOUNT\",\"value\":\"never-export\""+common+","
                    +"{\"domain\":\".youtube.com.evil.invalid\",\"name\":\"SPOOF\",\"value\":\"never-export\""+common+"]}}";
                byte[] message=payload.getBytes(StandardCharsets.UTF_8);
                output.write(129); output.write(126); output.write(message.length>>8); output.write(message.length&255); output.write(message); output.flush();
            }
            System.out.println("Scoped browser cookie protocol fixture passed");
        }
    }
}
