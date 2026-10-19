package dev.aether.export;

import java.io.File;
import java.util.Arrays;

public final class GameExportCli {
    public static void main(String[] args) throws Exception {
        if(args.length==2&&args[0].equals("--config")) {
            java.util.Properties config=new java.util.Properties();
            try(java.io.Reader reader=new java.io.InputStreamReader(new java.io.FileInputStream(args[1]),java.nio.charset.StandardCharsets.UTF_8)){config.load(reader);}
            String[] names={"template","project","output","applicationId","label","versionCode","versionName","keystore","storeType","alias"};
            args=new String[names.length];
            for(int i=0;i<names.length;++i){args[i]=config.getProperty(names[i]);if(args[i]==null)throw new IllegalArgumentException("Missing export setting: "+names[i]);}
        }
        if(args.length!=10)throw new IllegalArgumentException("Expected: template.apk project output.apk applicationId label versionCode versionName keystore storeType alias. Passwords: ASTRA_STORE_PASSWORD and ASTRA_KEY_PASSWORD environment variables.");
        String store=System.getenv("ASTRA_STORE_PASSWORD"),key=System.getenv("ASTRA_KEY_PASSWORD");
        if(store==null||key==null)throw new IllegalArgumentException("Signing password environment variables are required");
        char[] storeChars=store.toCharArray(),keyChars=key.toCharArray();
        try {
            ApkExporter.export(new File(args[0]),new File(args[1]),new File(args[2]),new File(System.getProperty("java.io.tmpdir")),
                new ApkExporter.Settings(args[3],args[4],Integer.parseInt(args[5]),args[6]),
                ApkExporter.signer(new File(args[7]),args[8],storeChars,args[9],keyChars),System.out::println);
        } finally {Arrays.fill(storeChars,'\0');Arrays.fill(keyChars,'\0');}
    }
}
