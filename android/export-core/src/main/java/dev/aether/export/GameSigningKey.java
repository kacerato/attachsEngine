package dev.aether.export;

import java.io.*;
import java.math.BigInteger;
import java.security.*;
import java.security.cert.X509Certificate;
import java.util.Date;
import org.bouncycastle.asn1.x500.X500Name;
import org.bouncycastle.cert.jcajce.JcaX509v3CertificateBuilder;
import org.bouncycastle.cert.jcajce.JcaX509CertificateConverter;
import org.bouncycastle.operator.jcajce.JcaContentSignerBuilder;

/** Portable, password-protected keys. No device-bound AndroidKeyStore key is silently substituted. */
public final class GameSigningKey {
    private GameSigningKey() {}
    public static void create(File output,String alias,char[] password) throws Exception {
        if(output.exists())throw new IOException("Signing key already exists");
        if(alias==null||alias.trim().isEmpty()||password.length<8)throw new IllegalArgumentException("Use an alias and a password of at least eight characters");
        KeyPairGenerator generator=KeyPairGenerator.getInstance("RSA");generator.initialize(3072);KeyPair pair=generator.generateKeyPair();
        long now=System.currentTimeMillis();X500Name subject=new X500Name("CN=Astra Game Signing");
        X509Certificate certificate=new JcaX509CertificateConverter().getCertificate(
            new JcaX509v3CertificateBuilder(subject,new BigInteger(160,new SecureRandom()).setBit(159),
                new Date(now-86400000L),new Date(now+40L*365*86400000L),subject,pair.getPublic())
                .build(new JcaContentSignerBuilder("SHA256withRSA").build(pair.getPrivate())));
        certificate.verify(pair.getPublic());
        KeyStore store=KeyStore.getInstance("PKCS12");store.load(null,password);
        store.setKeyEntry(alias,pair.getPrivate(),password,new java.security.cert.Certificate[]{certificate});
        try(OutputStream stream=java.nio.file.Files.newOutputStream(output.toPath(),java.nio.file.StandardOpenOption.CREATE_NEW)) {store.store(stream,password);}
    }
}
