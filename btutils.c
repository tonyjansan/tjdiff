#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

#include "btutils.h"
#include "libbzip2/bzlib.h"

inline off_t offtin(u_char *buf) {
	off_t y = buf[7] & 0x7F;
	y <<= 8; y += buf[6];
	y <<= 8; y += buf[5];
	y <<= 8; y += buf[4];
	y <<= 8; y += buf[3];
	y <<= 8; y += buf[2];
	y <<= 8; y += buf[1];
	y <<= 8; y += buf[0];
	return (buf[7] & 0x80)? -y : y;
}

inline void offtout(off_t x, u_char *buf) {
	off_t y = (x < 0)? -x : x;
	buf[0] = (u_char)y; y -= buf[0];
	y >>= 8; buf[1] = (u_char)y; y -= buf[1];
	y >>= 8; buf[2] = (u_char)y; y -= buf[2];
	y >>= 8; buf[3] = (u_char)y; y -= buf[3];
	y >>= 8; buf[4] = (u_char)y; y -= buf[4];
	y >>= 8; buf[5] = (u_char)y; y -= buf[5];
	y >>= 8; buf[6] = (u_char)y; y -= buf[6];
	y >>= 8; buf[7] = (u_char)y;
	if(x < 0) buf[7] |= 0x80;
}

static void split(off_t *I, off_t *V, off_t start, off_t len, off_t h) {
	off_t i, j, k, x, tmp, jj, kk;

	if(len < 16) {
		for(k = start; k < start + len; k += j) {
			j = 1; x = V[I[k] + h];
			for(i = 1; k + i < start + len; i++) {
				if(V[I[k + i] + h] < x) {
					x = V[I[k + i] + h];
					j = 0;
				}
				if(V[I[k + i] + h] == x) {
					tmp = I[k + j]; I[k + j] = I[k + i]; I[k + i] = tmp;
					j++;
				}
			}
			for(i = 0; i < j; i++) V[I[k + i]] = k + j - 1;
			if(j == 1) I[k] = -1;
		}
		return;
	}

	x = V[I[start + (len >> 1)] + h];
	jj = 0; kk = 0;
	for(i = start; i < start + len; i++) {
		if(V[I[i] + h] < x) jj++;
		if(V[I[i] + h] == x) kk++;
	}
	jj += start; kk += jj;

	i = start; j = 0; k = 0;
	while(i < jj) {
		if(V[I[i] + h] < x) {
			i++;
		} else if(V[I[i] + h]==x) {
			tmp = I[i]; I[i] = I[jj + j]; I[jj + j] = tmp;
			j++;
		} else {
			tmp = I[i]; I[i] = I[kk + k]; I[kk + k] = tmp;
			k++;
		}
	}

	while(jj + j < kk) {
		if(V[I[jj + j] + h] == x) {
			j++;
		} else {
			tmp = I[jj + j]; I[jj + j] = I[kk + k]; I[kk + k] = tmp;
			k++;
		}
	}

	if(jj > start) split(I, V, start, jj - start, h);

	for(i = 0; i < kk - jj; i++) V[I[jj + i]] = kk - 1;
	if(jj == kk - 1) I[jj] = -1;
	if(start + len > kk) split(I, V, kk, start + len - kk, h);
}

static void qsufsort(off_t *I, off_t *V, u_char *old, off_t oldsize) {
	off_t buckets[256];
	off_t i = 0, h, len;

	memset(buckets, 0, 256 * sizeof(off_t));
	for(; i < oldsize; i++) buckets[old[i]]++;
	for(i = 1; i < 256; i++) buckets[i] += buckets[i - 1];
	for(i = 255; i > 0; i--) buckets[i] = buckets[i - 1];
	buckets[0] = 0;

	for(i = 0; i < oldsize; i++) I[++buckets[old[i]]] = i;
	I[0] = oldsize;
	for(i = 0; i < oldsize; i++) V[i] = buckets[old[i]];
	V[oldsize] = 0;
	for(i = 1; i < 256; i++)
		if(buckets[i] == buckets[i - 1] + 1) I[buckets[i]] = -1;
	I[0] = -1;

	for(h = 1; I[0] != -(oldsize + 1); h += h) {
		len = 0;
		for(i = 0; i < oldsize + 1; ) {
			if(I[i] < 0) {
				len -= I[i];
				i -= I[i];
			} else {
				if(len) I[i - len] = -len;
				len = V[I[i]] + 1 - i;
				split(I, V, i, len, h);
				i += len;
				len = 0;
			}
		}
		if(len) I[i - len] = -len;
	}

	for(i = 0; i < oldsize + 1; i++) I[V[i]] = i;
}

static off_t matchlength(u_char *old, off_t oldsize, u_char *new, off_t newsize) {
	off_t i = 0, min = MIN(oldsize, newsize);
	for(; i < min && old[i] == new[i]; i++);
	return i;
}

static off_t search(off_t *I, u_char *old, off_t oldsize,
    u_char *new, off_t newsize, off_t st, off_t en, off_t *pos) {
	off_t x, y;
	while(en - st > 1) {
		x = st + ((en - st) >> 1);
		if(memcmp(old + I[x], new, MIN(oldsize - I[x], newsize)) < 0)
			st = x;
		else
			en = x;
	}

	x = matchlength(old + I[st], oldsize - I[st], new, newsize);
	y = matchlength(old + I[en], oldsize - I[en], new, newsize);
	if(x > y) {
		*pos = I[st];
		return x;
	} else {
		*pos = I[en];
		return y;
	}
}

// Notes: The method allocates memory for data, which should be free if unused.
off_t getfiledata(const char *path, u_char **pdata) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return -1;

	off_t size = getfilesize(f), ret = -1;
	if (size >= 0) {
		/* Allocate size + 1 bytes instead of size bytes to ensure
			that we never try to malloc(0) and get a NULL pointer */
		*pdata = (u_char*)malloc(size + 1);
		if (*pdata) {
			ret = fread(*pdata, sizeof(u_char), size, f);
			if (ret < size) {
				free(*pdata); *pdata = 0;
				ret = -1;
			}
		}
	}
	free(f);
	return ret;
}

int btdiff(const char* srcfile, const char* dstfile, const char* difffile)
{
	off_t *I, *V;
	u_char *old = 0, *new = 0;
	off_t oldsize, newsize;
	off_t scan = 0, pos = 0, len = 0;
	off_t lastscan = 0, lastpos = 0, lastoffset = 0;
	off_t oldscore, scsc;
	off_t s, Sf, lenf, Sb, lenb, overlap, Ss, lens;
	off_t i;
	off_t dblen = 0, eblen = 0;
	u_char *db, *eb;
	u_char buf[8];
	u_char header[32];
	FILE *f;
	BZFILE *pfbz2;
	int bz2err;

	if ((oldsize = getfiledata(srcfile, &old)) < 0) {
		print_log_ex("read %s error!\n", srcfile);
		return 0;
	}

	if(!(I = malloc((oldsize + 1) * sizeof(off_t)))) {
		print_log("malloc memory for I error!\n");
		free(old);
		return 0;
	}
	if (!(V = malloc((oldsize + 1) * sizeof(off_t)))) {
		print_log("malloc memory for V error!\n");
		free(I); free(old);
		return 0;
	}

	qsufsort(I, V, old, oldsize);
	free(V);

	if ((newsize = getfiledata(dstfile, &new)) < 0) {
		print_log_ex("read %s error!\n", dstfile);
		free(I); free(old);
		return 0;
	}

	if(!(db = malloc(newsize + 1))) {
		print_log("malloc memory for db error!\n");
		free(new); free(I); free(old);
		return 0;
	}
	if (!(eb = malloc(newsize + 1))) {
		print_log("malloc memory for eb error!\n");
		free(db); free(new); free(I); free(old);
		return 0;
	}

	/* Create the patch file */
	if (!(f = fopen(difffile, "wb"))) {
		print_log_ex("open %s error!\n", difffile);
		free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}

	/* Header is
		0	8	 "DIFFBZIP"
		8	8	length of bzip2ed ctrl block
		16	8	length of bzip2ed diff block
		24	8	length of new file */
	/* File is
		0	32	Header
		32	??	Bzip2ed ctrl block
		??	??	Bzip2ed diff block
		??	??	Bzip2ed extra block */
	memcpy(header, "DIFFBZIP", 8);
	offtout(0, header + 8);
	offtout(0, header + 16);
	offtout(newsize, header + 24);
	if (fwrite(header, sizeof(u_char), 32, f) < 32) {
		print_log_ex("fwrite(%s) error!\n", difffile);
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}

	/* Compute the differences, writing ctrl as we go */
	if (!(pfbz2 = BZ2_bzWriteOpen(&bz2err, f, 9, 0, 0))) {
		print_log_ex("BZ2_bzWriteOpen error, bz2err = %d\n", bz2err);
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}

	while(scan < newsize) {
		oldscore = 0;
		for(scsc = scan += len; scan < newsize; scan++) {
			len = search(I, old, oldsize, new + scan, newsize - scan, 0, oldsize, &pos);
			for(; scsc < scan + len; scsc++)
				if((scsc + lastoffset < oldsize) && (old[scsc + lastoffset] == new[scsc]))
					oldscore++;
			if(((len == oldscore) && len) || (len > oldscore + 8)) break;
			if((scan + lastoffset < oldsize) && (old[scan + lastoffset] == new[scan]))
				oldscore--;
		}

		if((len != oldscore) || (scan == newsize)) {
			s = 0; Sf = 0; lenf = 0;
			for(i = 0; (lastscan + i < scan) && (lastpos + i < oldsize); ) {
				if(old[lastpos + i] == new[lastscan + i]) s++;
				i++;
				if(s * 2 - i > Sf * 2 - lenf) { Sf = s; lenf = i; }
			}

			lenb = 0;
			if(scan < newsize) {
				s = 0; Sb = 0;
				for(i = 1; (scan >= lastscan + i) && (pos >= i); i++) {
					if(old[pos - i] == new[scan - i]) s++;
					if(s * 2 - i > Sb * 2 - lenb) { Sb = s; lenb = i; }
				}
			}

			if(lastscan + lenf > scan - lenb) {
				overlap = (lastscan + lenf) - (scan - lenb);
				s = 0; Ss = 0; lens = 0;
				for(i = 0; i < overlap; i++) {
					if(new[lastscan + lenf - overlap + i] == old[lastpos + lenf - overlap + i])
						s++;
					if(new[scan - lenb + i] == old[pos - lenb + i])
						s--;
					if(s > Ss) { Ss = s; lens = i + 1; }
				}
				lenf += lens - overlap;
				lenb -= lens;
			}

			for(i = 0; i < lenf; i++)
				db[dblen + i] = new[lastscan + i] - old[lastpos + i];
			memcpy(eb + eblen, new + lastscan + lenf, scan - lenb - lastscan - lenf);

			dblen += lenf;
			eblen += scan - lenb - lastscan - lenf;

			offtout(lenf, buf);
			BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
			if (bz2err != BZ_OK) {
				print_log_ex("BZ2_bzWrite error, bz2err = %d\n", bz2err);
				BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
				fclose(f); free(eb); free(db); free(new); free(I); free(old);
				return 0;
			}

			offtout(scan - lenb - lastscan - lenf, buf);
			BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
			if (bz2err != BZ_OK) {
				print_log_ex("BZ2_bzWrite error, bz2err = %d\n", bz2err);
				BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
				fclose(f); free(eb); free(db); free(new); free(I); free(old);
				return 0;
			}

			offtout(pos - lenb - lastpos - lenf, buf);
			BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
			if (bz2err != BZ_OK) {
				print_log_ex("BZ2_bzWrite error, bz2err = %d\n", bz2err);
				BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
				fclose(f); free(eb); free(db); free(new); free(I); free(old);
				return 0;
			}

			lastscan = scan - lenb;
			lastpos = pos - lenb;
			lastoffset = pos - scan;
		}
	}
	BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
	if (bz2err != BZ_OK) {
		print_log_ex("BZ2_bzWriteClose error, bz2err = %d\n", bz2err);
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}

	/* Compute size of compressed ctrl data */
	if ((len = ftell(f)) < 0) {
		print_log("compute ctrl-block size error!\n");
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}
	offtout(len - 32, header + 8);

	/* Write compressed diff data */
	if (!(pfbz2 = BZ2_bzWriteOpen(&bz2err, f, 9, 0, 0))) {
		print_log_ex("write diff-data error! %s\n", "bzWriteOpen()");
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}
	BZ2_bzWrite(&bz2err, pfbz2, db, dblen);
	if (bz2err != BZ_OK) {
		print_log_ex("write diff-data error! %s\n", "bzWrite");
		BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}
	BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
	if (bz2err != BZ_OK) {
		print_log_ex("write diff-data error! %s\n", "bzWriteClose");
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}

	/* Compute size of compressed diff data */
	if ((newsize = ftell(f)) < 0) {
		print_log("compute diff-block size error!");
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}
	offtout(newsize - len, header + 16);

	/* Write compressed extra data */
	if (!(pfbz2 = BZ2_bzWriteOpen(&bz2err, f, 9, 0, 0))) {
		print_log_ex("write extra-data error! %s\n", "bzWriteOpen()");
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}
	BZ2_bzWrite(&bz2err, pfbz2, eb, eblen);
	if (bz2err != BZ_OK) {
		print_log_ex("write extra-data error! %s\n", "bzWrite");
		BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}
	BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
	if (bz2err != BZ_OK) {
		print_log_ex("write extra-data error! %s\n", "bzWriteClose");
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}

	len = ftell(f);
	/* Seek to the beginning, write the header, and close the file */
	if (fseek(f, 0, SEEK_SET) || fwrite(header, sizeof(u_char), 32, f) < 32) {
		print_log("write diff-header error!");
		fclose(f); free(eb); free(db); free(new); free(I); free(old);
		return 0;
	}

	if (fclose(f))
		print_log("Warning! Close diff-file error!");
	free(eb); free(db); free(new); free(I); free(old);

	return 1;
}

int btpatch(const char* srcfile, const char* dstfile, const char* difffile)
{
	FILE *f, *cpf, *dpf, *epf;
	BZFILE *cpfbz2, *dpfbz2, *epfbz2;
	int cbz2err, dbz2err, ebz2err;
	ssize_t oldsize, newsize;
	ssize_t bzctrllen, bzdatalen;
	u_char header[32], buf[8];
	u_char *old, *new;
	off_t oldpos, newpos;
	off_t ctrl[3];
	off_t lenread, i;

	/* Open patch file */
	if (!(f = fopen(difffile, "rb"))) {
		print_log_ex("fopen(%s) error!", difffile);
		return 0;
	}

	/*
	File format:
		0	8	"DIFFBZIP"
		8	8	X
		16	8	Y
		24	8	sizeof(newfile)
		32	X	bzip2(control block)
		32+X	Y	bzip2(diff block)
		32+X+Y	???	bzip2(extra block)
	with control block a set of triples (x,y,z) meaning "add x bytes
	from oldfile to x bytes from the diff block; copy y bytes from the
	extra block; seek forwards in oldfile by z bytes".
	*/

	/* Read header */
	if (fread(header, sizeof(u_char), 32, f) < 32) {
		print_log_ex("fread(%s) error!", difffile);
		fclose(f);
		return 0;
	}

	/* Check for appropriate magic */
	if (memcmp(header, "DIFFBZIP", 8)) {
		print_log("Corrupt patch\n");
		fclose(f);
		return 0;
	}

	/* Read lengths from header */
	bzctrllen = offtin(header + 8);
	bzdatalen = offtin(header + 16);
	newsize = offtin(header + 24);
	if((bzctrllen < 0) || (bzdatalen < 0) || (newsize < 0)) {
		print_log("Corrupt patch\n");
		fclose(f);
		return 0;
	}

	/* Close patch file and re-open it via libbzip2 at the right places */
	if (fclose(f))
		return 0;
	if (!(cpf = fopen(difffile, "rb")))
		return 0;
	if (fseek(cpf, 32, SEEK_SET)) {
		fclose(cpf);
		return 0;
	}
	if (!(cpfbz2 = BZ2_bzReadOpen(&cbz2err, cpf, 0, 0, NULL, 0))) {
		fclose(cpf);
		return 0;
	}
	if (!(dpf = fopen(difffile, "rb"))) {
		BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
		return 0;
	}
	if (fseek(dpf, 32 + bzctrllen, SEEK_SET)) {
		fclose(dpf);
		BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
		return 0;
	}
	if (!(dpfbz2 = BZ2_bzReadOpen(&dbz2err, dpf, 0, 0, NULL, 0))) {
		fclose(dpf);
		BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
		return 0;
	}
	if (!(epf = fopen(difffile, "rb"))) {
		BZ2_bzReadClose(&dbz2err, dpfbz2); fclose(dpf);
		BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
		return 0;
	}
	if (fseek(epf, 32 + bzctrllen + bzdatalen, SEEK_SET)) {
		fclose(epf);
		BZ2_bzReadClose(&dbz2err, dpfbz2); fclose(dpf);
		BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
		return 0;
	}
	if (!(epfbz2 = BZ2_bzReadOpen(&ebz2err, epf, 0, 0, NULL, 0))) {
		fclose(epf);
		BZ2_bzReadClose(&dbz2err, dpfbz2); fclose(dpf);
		BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
		return 0;
	}

	if ((oldsize = getfiledata(srcfile, &old)) < 0) {
		print_log_ex("read %s error!", srcfile);
		BZ2_bzReadClose(&ebz2err, epfbz2); fclose(epf);
		BZ2_bzReadClose(&dbz2err, dpfbz2); fclose(dpf);
		BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
		return 0;
	}
	if(!(new = malloc(newsize + 1))) {
		print_log("allocate memory for new error!");
		free(old);
		BZ2_bzReadClose(&ebz2err, epfbz2); fclose(epf);
		BZ2_bzReadClose(&dbz2err, dpfbz2); fclose(dpf);
		BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
		return 0;
	}

	oldpos = 0; newpos = 0;
	while(newpos < newsize) {
		/* Read control data */
		for(i = 0; i <= 2; i++) {
			lenread = BZ2_bzRead(&cbz2err, cpfbz2, buf, 8);
			if ((lenread < 8) || ((cbz2err != BZ_OK) &&
			    (cbz2err != BZ_STREAM_END))) {
				print_log("read control-block error!");
				free(new); free(old);
				BZ2_bzReadClose(&ebz2err, epfbz2); fclose(epf);
				BZ2_bzReadClose(&dbz2err, dpfbz2); fclose(dpf);
				BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
				return 0;
			}
			ctrl[i] = offtin(buf);
		};

		/* Sanity-check */
		if(newpos + ctrl[0] > newsize) {
			print_log("control-block(0) sanity-check error!");
			free(new); free(old);
			BZ2_bzReadClose(&ebz2err, epfbz2); fclose(epf);
			BZ2_bzReadClose(&dbz2err, dpfbz2); fclose(dpf);
			BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
			return 0;
		}

		/* Read diff string */
		lenread = BZ2_bzRead(&dbz2err, dpfbz2, new + newpos, ctrl[0]);
		if ((lenread < ctrl[0]) ||
		    ((dbz2err != BZ_OK) && (dbz2err != BZ_STREAM_END))) {
			print_log("read diff-block error!");
			free(new); free(old);
			BZ2_bzReadClose(&ebz2err, epfbz2); fclose(epf);
			BZ2_bzReadClose(&dbz2err, dpfbz2); fclose(dpf);
			BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
			return 0;
		}

		/* Add old data to diff string */
		for(i = 0; i < ctrl[0]; i++)
			if((oldpos + i >= 0) && (oldpos + i < oldsize))
				new[newpos + i] += old[oldpos + i];

		/* Adjust pointers */
		newpos += ctrl[0]; oldpos += ctrl[0];

		/* Sanity-check */
		if(newpos + ctrl[1] > newsize) {
			print_log("control-block(1) sanity-check error!");
			free(new); free(old);
			BZ2_bzReadClose(&ebz2err, epfbz2); fclose(epf);
			BZ2_bzReadClose(&dbz2err, dpfbz2); fclose(dpf);
			BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
			return 0;
		}

		/* Read extra string */
		lenread = BZ2_bzRead(&ebz2err, epfbz2, new + newpos, ctrl[1]);
		if ((lenread < ctrl[1]) ||
		    ((ebz2err != BZ_OK) && (ebz2err != BZ_STREAM_END))) {
			print_log("read extra-block error!");
			free(new); free(old);
			BZ2_bzReadClose(&ebz2err, epfbz2); fclose(epf);
			BZ2_bzReadClose(&dbz2err, dpfbz2); fclose(dpf);
			BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);
			return 0;
		}

		/* Adjust pointers */
		newpos += ctrl[1]; oldpos += ctrl[2];
	};

	/* Clean up the bzip2 reads */
	free(old);
	BZ2_bzReadClose(&ebz2err, epfbz2); fclose(epf);
	BZ2_bzReadClose(&dbz2err, dpfbz2); fclose(dpf);
	BZ2_bzReadClose(&cbz2err, cpfbz2); fclose(cpf);

	/* Write the new file */
	if(!(f = fopen(dstfile, "wb"))) {
		print_log_ex("write %s error!\n", dstfile);
		free(new);
		return 0;
	}
	if(fwrite(new, sizeof(char), newsize, f) < newsize) {
		print_log_ex("write %s error!\n", dstfile);
		fclose(f);
		free(new);
		return 0;
	}
	if(fclose(f))
		print_log_ex("Warning! close %s error!", dstfile);

	free(new);
	return 0;
}