Thunar Sendto Email (thunar-sendto-email)
=========================================

Thunar-sendto-email is an extension to Thunar, which adds an additional entry to the "Send To" sub menu, named "Mail Recipient", that starts the mail composer and attaches the selected files (using the exo-open mechanism).

The extension uses the zip command to compress folders prior to sending them to the mail client, since most mail clients cannot handle directories as attachments. For regular files, larger than 200KiB, the extension prompts the user whether to compress the files (using the zip command) prior to sending them to the mail client.
