/* Constraint #4: both gdk_pixbuf PNG ("Read Error") and xfwm4's XPM reader ("Cannot read Pixmap header")
 * fail to read the small vm-tree-staged decoration files via stdio. Test raw stdio fread/fseek on such a
 * file to find the wasi-libc stdio bug (short reads? fseek-after-fread buffer not reset?). Reports the
 * byte counts so we can see exactly where stdio diverges from the real file size. */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

static void say(const char *m) { write(2, m, strlen(m)); }

/* Raw POSIX open/read/lseek/read -- bypasses the stdio buffer. Discriminates wasi-libc-stdio (above the
 * WASI fd layer) from the runner's fd_seek/fd_read (the WASI layer itself). */
static void raw_probe(const char *path) {
    char b[256];
    int fd = open(path, O_RDONLY);
    if (fd < 0) { say("RAW: open FAILED\n"); return; }
    char buf[16];
    ssize_t r1 = read(fd, buf, 8);
    off_t s = lseek(fd, 0, SEEK_SET);
    ssize_t r2 = read(fd, buf, 8);
    off_t cur = lseek(fd, 0, SEEK_CUR);
    snprintf(b, sizeof b, "RAW: %s read1=%zd lseek0=%lld read2=%zd cur_after=%lld\n",
             path, r1, (long long)s, r2, (long long)cur); say(b);
    close(fd);
}

static void probe(const char *path) {
    char b[256];
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(b, sizeof b, "FRP: fopen FAILED %s\n", path); say(b); return; }
    /* full sequential read */
    unsigned char buf[8192]; size_t total = 0, n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) total += n;
    snprintf(b, sizeof b, "FRP: %s sequential fread total=%zu eof=%d err=%d\n", path, total, feof(f), ferror(f)); say(b);
    /* rewind via fseek and re-read first 16 bytes (xfwm4/gdk_pixbuf sniff-then-rewind pattern) */
    clearerr(f);
    if (fseek(f, 0, SEEK_SET) != 0) { say("FRP: fseek(0) FAILED\n"); }
    size_t n2 = fread(buf, 1, 16, f);
    snprintf(b, sizeof b, "FRP: after fseek(0) reread16=%zu  first8=%02x %02x %02x %02x %02x %02x %02x %02x\n",
             n2, buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]); say(b);
    /* sniff-then-rewind: read 8, fseek back, read 8 again -- the exact fseek-after-fread case */
    fseek(f, 0, SEEK_SET);
    unsigned char s1[8]; size_t a = fread(s1, 1, 8, f);
    fseek(f, 0, SEEK_SET);
    unsigned char s2[8]; size_t c = fread(s2, 1, 8, f);
    snprintf(b, sizeof b, "FRP: sniff a=%zu c=%zu match=%d (s1[0..3]=%02x%02x%02x%02x s2=%02x%02x%02x%02x)\n",
             a, c, memcmp(s1,s2,8)==0, s1[0],s1[1],s1[2],s1[3], s2[0],s2[1],s2[2],s2[3]); say(b);
    fclose(f);
}

int main(void) {
    say("FRP: start\n");
    raw_probe("/usr/share/themes/Greybird/xfwm4/close-active.png");
    probe("/usr/share/themes/Greybird/xfwm4/close-active.png");   /* 315 bytes RGBA */
    probe("/usr/share/themes/Greybird/xfwm4/title-1-active.png"); /* 156 bytes LA */
    say("FRP: done\n");
    return 0;
}
