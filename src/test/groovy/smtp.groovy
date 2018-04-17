#!/usr/bin/env groovy

@Grab(group = 'com.sun.mail', module = 'javax.mail', version = '1.6.0')

import javax.mail.Message
import javax.mail.Session
import javax.mail.Transport
import javax.mail.internet.InternetAddress
import javax.mail.internet.MimeMessage

MAILER_HOST = "smtp.mailtrap.io"
MAILER_USER = "customValueHere"
MAILER_PASS = "sekretValueHere"
MAILER_PORT = 2525
RECIPIENT_EMAIL = "works4me@example.com"

private void runScript() {
    Session session = Session.getDefaultInstance(new Properties())

    MimeMessage message = new MimeMessage(session)
    message.setFrom("no.reply@example.org")
    message.setRecipient(Message.RecipientType.TO, new InternetAddress(RECIPIENT_EMAIL))
    message.setSubject("A Test message...")
    message.setText("This is a sample email message!")

    Transport transport = session.getTransport("smtp")
    transport.connect(MAILER_HOST, MAILER_PORT, MAILER_USER, MAILER_PASS)
    transport.sendMessage(message, message.getAllRecipients())
    transport.close()
}

runScript()

