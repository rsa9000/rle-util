/**
 * Simple RLE (Run-Lenght Encoding) encoding/decoding utility
 *
 * Copyright (c) 2017 Sergey Ryazanov <ryazanov.s.a@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * RLE algorithm: https://en.wikipedia.org/wiki/Run-length_encoding
 *
 * Compressed format: each run of bytes is preceded by an unsigned octet LEN,
 * if LEN < 128, then the next byte will be repeated LEN times in the output
 * stream, if LEN >= 128 then the subsequent (256 - LEN) bytes will be copied
 * to the output stream "as is".
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <libgen.h>

#include <sys/types.h>
#include <sys/stat.h>

#define PR_ERR(__fmt, ...)					\
		fprintf(stderr, "rle: " __fmt "\n", ## __VA_ARGS__)
#define PR_ERR_SYS(__fmt, ...)					\
		PR_ERR(__fmt ": %s", ## __VA_ARGS__, strerror(errno))
#define PR_ERR_SYS_ONLY()					\
		PR_ERR("%s", strerror(errno))

static const char utility_version[] = "1.0-beta";

static int decode(FILE *fin, FILE *fout)
{
	int ch, len = 0;

	while ((ch = fgetc(fin)) != EOF) {
		if (!len) {
			if (!ch)
				break;

			len = ch;
		} else if (len < 0x80) {
			do {
				if (fputc(ch, fout) == EOF)
					break;
			} while (--len);
		} else {
			if (fputc(ch, fout) == EOF)
				break;
			len = (len + 1) % 0x100;
		}
	}

	if (ferror(fin)) {
		PR_ERR("Input error.");
		return -EIO;
	} else if (ferror(fout)) {
		PR_ERR("Output error.");
		return -EIO;
	} else if (len) {
		PR_ERR("Unexpected end of stream.");
		return -EIO;
	}

	return 0;
}

static int encode(FILE *fin, FILE *fout)
{
	int ch = 0, len = 0, flush = 0, repeat = 0;
	uint8_t buf[0x80 + 1];

	while (ch != EOF) {
		ch = fgetc(fin);
		if (ch != EOF)
			buf[len++] = ch;
		else if (ferror(fin))
			break;

		if (repeat) {
			/* There are always at least 2 bytes in buffer */
			if (buf[len - 2] != buf[len - 1]) {
				flush = len - 1;
				repeat = 0;
			} else if (len == 0x7f || ch == EOF) {
				flush = len;
				repeat = 0;
			}
			if (flush) {
				fputc(flush, fout);
				fputc(buf[0], fout);
			}
		} else {
			if (len >= 2 && buf[len - 2] == buf[len - 1]) {
				flush = len - 2;
				repeat = 1;
			} else if (len == 0x81) {
				flush = 0x80;
			} else if (ch == EOF) {
				flush = len;
			}
			if (flush) {
				fputc(0x100 - flush, fout);
				fwrite(buf, flush, 1, fout);
			}
		}
		if (flush) {
			if (ferror(fout))
				break;
			memmove(&buf[0], &buf[flush], len - flush);
			len -= flush;
			flush = 0;
		}
	}

	if (ferror(fin)) {
		PR_ERR("Input error.");
		return -EIO;
	} else if (ferror(fout)) {
		PR_ERR("Output error.");
		return -EIO;
	}

	return 0;
}

static void usage(const char *name)
{
	printf(
		"Usage: %s [OPTION]... [FILE]\n"
		"Compress or decompress FILE in the .rle format.\n"
		"\n"
		"  -c         write to standard output and do not delete input file\n"
		"  -d         force decompression (decoding)\n"
		"  -f         force (file overwrite, output to a terminal, etc.)\n"
		"  -h         display this help and exit\n"
		"  -k         keep (do not delete) input file\n"
		"  -V         display the version and exit\n"
		"\n"
		"With no FILE, or when FILE is -, read standard input and write standard output.\n"
		"\n"
		"Written by Sergey Ryazanov <ryazanov.s.a@gmail.com>\n",
		name
	);
}

int main(int argc, char *argv[])
{
	const char *execname = basename(argv[0]);
	int (*op)(FILE *, FILE *) = encode;
	int force = 0, keep = 0;
	char *fnin, *fnout = NULL;
	size_t fnlen;
	int fdout;
	FILE *fin = NULL;
	FILE *fout = NULL;
	int ret = -EINVAL, opt;

	while ((opt = getopt(argc, argv, "cdfhkV")) != -1) {
		switch (opt) {
		case 'c':
			fout = stdout;
			break;
		case 'd':
			op = decode;
			break;
		case 'f':
			force = 1;
			break;
		case 'h':
			usage(execname);
			return EXIT_SUCCESS;
		case 'k':
			keep = 1;
			break;
		case 'V':
			printf("rle %s\n", utility_version);
			return EXIT_SUCCESS;
		default:
			return EXIT_FAILURE;
		}
	}

	if (optind == argc) {
		fnin = "-";
	} else {
		fnin = argv[optind];
	}

	if (strcmp(fnin, "-") == 0) {
		keep = 1;
		fin = stdin;
		fout = stdout;
	}

	if (!fin) {
		fin = fopen(fnin, "rb");
		if (!fin) {
			PR_ERR_SYS("%s", fnin);
			goto exit;
		}
	}

	if (!fout) {
		fnlen = strlen(fnin);
		if (op == encode) {
			if (!force && fnlen > 4 &&
			    !strcmp(&fnin[fnlen - 4], ".rle")) {
				PR_ERR("%s: Filename already has `.rle' suffix",
				       fnin);
				goto exit;
			}
			fnout = malloc(fnlen + 4 + 1);
			if (!fnout) {
				PR_ERR_SYS_ONLY();
				goto exit;
			}
			sprintf(fnout, "%s.rle", fnin);
		} else {
			if (fnlen <= 4 || strcmp(&fnin[fnlen - 4], ".rle")) {
				PR_ERR("%s: Filename has an unknown suffix",
				       fnin);
				goto exit;
			}
			fnout = malloc(fnlen - 4 + 1);
			if (!fnout) {
				PR_ERR_SYS_ONLY();
				goto exit;
			}
			memcpy(fnout, fnin, fnlen - 4);
			fnout[fnlen - 4] = '\0';
		}

		if (force && unlink(fnout) && errno != ENOENT) {
			PR_ERR_SYS("%s: Can not remove", fnout);
			goto exit;
		}

		fdout = open(fnout, O_WRONLY | O_NOCTTY | O_CREAT | O_EXCL,
			     S_IRUSR | S_IWUSR);
		if (fdout == -1) {
			PR_ERR_SYS("%s", fnout);
			goto exit;
		}

		fout = fdopen(fdout, "wb");
		if (!fout) {
			close(fdout);
			PR_ERR_SYS_ONLY();
			goto exit;
		}
	}

	if (op == encode && isatty(fileno(fout)) && !force) {
		PR_ERR("Compressed data can not be written to a terminal.");
		PR_ERR("Try `%s -h' for more information.", execname);
		goto exit;
	}

	ret = op(fin, fout);

	if (!ret && !keep) {
		fclose(fin);
		fin = NULL;
		if (unlink(fnin)) {
			PR_ERR_SYS("%s: Can not remove", fnin);
			ret = -errno;
		}
	}

exit:
	if (fin && fin != stdin)
		fclose(fin);
	if (fout && fout != stdout)
		fclose(fout);
	free(fnout);

	return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
