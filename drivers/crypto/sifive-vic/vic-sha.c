/*
 ******************************************************************************
 * @file  vic-sha.c
 * @author  StarFive Technology
 * @version  V1.0
 * @date  08/13/2020
 * @brief
 ******************************************************************************
 * @copy
 *
 * THE PRESENT SOFTWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STARFIVE SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * <h2><center>&copy; COPYRIGHT 2020 Shanghai StarFive Technology Co., Ltd. </center></h2>
 */

#include <linux/clk.h>
#include <linux/crypto.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include <crypto/engine.h>
#include <crypto/hash.h>
#include <crypto/md5.h>
#include <crypto/scatterwalk.h>

#include <crypto/internal/hash.h>

#include "vic-sec.h"

#define HASH_IE          0x00
#define HASH_STATUS      0x04
#define HASH_CTRL        0x80
#define HASH_CFG        0x84

#define SHA_MSG_RAM_OFFSET  (16*1024)
#define SHA_MSG_RAM_SIZE    (8*1024)
#define SHA_CTX_RAM_OFFSET  (SHA_MSG_RAM_OFFSET + SHA_MSG_RAM_SIZE)
#define SHA_CTX_RAM_SIZE    (4*1024)
#define SHA_SEC_RAM_OFFSET  (SHA_CTX_RAM_OFFSET + SHA_CTX_RAM_SIZE)
#define SHA_SEC_RAM_SIZE    (2*1024)


#define HASH_FLAGS_INIT			BIT(0)
#define HASH_FLAGS_FINAL		BIT(3)
#define HASH_FLAGS_FINUP		BIT(4)

#define HASH_FLAGS_ALGO_MASK		GENMASK(8, 13)
#define HASH_FLAGS_MD5			BIT(8)
#define HASH_FLAGS_SHA1			BIT(9)
#define HASH_FLAGS_SHA224		BIT(10)
#define HASH_FLAGS_SHA256		BIT(11)
#define HASH_FLAGS_SHA384		BIT(12)
#define HASH_FLAGS_SHA512		BIT(13)
#define HASH_FLAGS_ERRORS		BIT(14)
#define HASH_FLAGS_HMAC			BIT(15)

static inline int vic_hash_wait_busy(struct vic_sec_dev *hdev)
{
	int ret = -1;

	mutex_lock(&hdev->doing);
	if(hdev->status.sec_done && (!hdev->status.sha_busy))
		ret = 0;
	mutex_unlock(&hdev->doing);
	return ret;
	//return wait_cond_timeout(hdev->status.sec_done && (!hdev->status.sha_busy), 10, 10000);
}

static int vic_hash_write_key(struct vic_sec_dev *hdev)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(hdev->req);
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(tfm);
	int keylen = ctx->keylen;
	void *key = ctx->key;

	if (keylen) {
		vic_write_n(hdev->io_base + SHA_SEC_RAM_OFFSET, key, keylen);
	}

	return 0;
}

static void vic_hash_append_sg(struct vic_sec_request_ctx *rctx)
{
	size_t count;

	while ((rctx->bufcnt < rctx->buflen) && rctx->total) {
		count = min(rctx->sg->length - rctx->offset, rctx->total);
		count = min(count, rctx->buflen - rctx->bufcnt);

		if (count <= 0) {
			if ((rctx->sg->length == 0) && !sg_is_last(rctx->sg)) {
				rctx->sg = sg_next(rctx->sg);
				continue;
			} else {
				break;
			}
		}

		scatterwalk_map_and_copy(rctx->buffer + rctx->bufcnt, rctx->sg,
					 rctx->offset, count, 0);

		rctx->bufcnt += count;
		rctx->offset += count;
		rctx->total -= count;

		if (rctx->offset == rctx->sg->length) {
			rctx->sg = sg_next(rctx->sg);
			if (rctx->sg)
				rctx->offset = 0;
			else
				rctx->total = 0;
		}
	}
}

static int vic_sha_start(struct vic_sec_request_ctx *rctx)
{
	struct vic_sec_dev *sdev = rctx->sdev;
	int loop, int_len = sizeof(unsigned int);

	if(sdev->status.sha_busy) {
		return -EBUSY;
	}

	for(loop = 0; loop < CFG_REGS_LEN / int_len; loop++) {
		writel(*(rctx->sha_cfg.vs + loop), sdev->io_base + HASH_CFG + loop * int_len);
	}
	sdev->ie.sec_done_ie = 1;
	mutex_lock(&sdev->doing);

	writel(sdev->ie.v, sdev->io_base + HASH_IE);
	sdev->status.sha_busy = 1;
	sdev->status.sec_done = 0;
	rctx->sha_ctrl.sha_start = 1;
	writel(rctx->sha_ctrl.v, sdev->io_base + HASH_CTRL);

	return 0;
}

static int vic_hash_xmit_cpu(struct vic_sec_dev *sdev,
			       struct vic_sec_request_ctx *rctx, int final)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(sdev->req);
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(tfm);
	int length = rctx->bufcnt;
	int ret = -EINPROGRESS;

	memset(&rctx->sha_cfg,0,sizeof(rctx->sha_cfg));
	memset(&rctx->sha_ctrl,0,sizeof(rctx->sha_ctrl));

	if (final) {
		if(rctx->msg_tot){
			rctx->sha_cfg.sha_num_bytes = length;
			rctx->sha_cfg.sha_tot_bytes = rctx->msg_tot + length;
			rctx->sha_cfg.sha_store_ctx_2msg = 1;
			rctx->sha_cfg.sha_ctx_msg_addr = SHA_CTX_MSG_ADDR;
			rctx->sha_cfg.sha_ctx_idx = 0;
			rctx->sha_ctrl.sha_mode = rctx->mode;
			rctx->sha_ctrl.sha_msg_end = 1;
			rctx->sha_ctrl.sha_retrieve_ctx = 1;
		} else {
			rctx->sha_cfg.sha_num_bytes = length;
			rctx->sha_cfg.sha_tot_bytes = length;
			rctx->sha_cfg.sha_store_ctx_2msg = 1;
			rctx->sha_cfg.sha_ctx_msg_addr = SHA_CTX_MSG_ADDR;
			rctx->sha_cfg.sha_ctx_idx = 0;

			rctx->sha_ctrl.sha_mode = rctx->mode;
			rctx->sha_ctrl.sha_msg_begin = 1;
			rctx->sha_ctrl.sha_msg_end = 1;
			rctx->sha_ctrl.sha_store_ctx = 1;
		}
		sdev->flags |= HASH_FLAGS_FINAL;
		ret = 0;
	} else {
		rctx->msg_tot += length;

		rctx->sha_cfg.sha_num_bytes = length;
		rctx->sha_cfg.sha_ctx_idx = 0;

		rctx->sha_ctrl.sha_store_ctx = 1;
		rctx->sha_ctrl.sha_mode = rctx->mode;
		if(rctx->last_block_idx == 0){
			rctx->sha_ctrl.sha_msg_begin = 1;
		} else {
			rctx->sha_ctrl.sha_retrieve_ctx = 1;
		}

		rctx->last_block_idx = 1;
	}

	if (rctx->flags & HASH_FLAGS_HMAC) {
		rctx->sha_cfg.sha_secret_bytes = ctx->keylen;
		rctx->sha_ctrl.sha_hmac = 1;
	}

	// put_msg
	vic_write_n(sdev->io_base + SHA_MSG_RAM_OFFSET, rctx->buffer, rctx->bufcnt);

	//set key
	vic_hash_write_key(sdev);

	//start
	vic_sha_start(rctx);

	//wait();
	if(vic_hash_wait_busy(sdev))
		ret = -ETIMEDOUT;

	return ret;
}

static int vic_hash_update_cpu(struct vic_sec_dev *hdev)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(hdev->req);
	int err = 0, final;

	dev_dbg(hdev->dev, "%s flags %lx\n", __func__, rctx->flags);

	final = (rctx->flags & HASH_FLAGS_FINUP);

	while ((rctx->total >= rctx->buflen) ||
	       (rctx->bufcnt + rctx->total >= rctx->buflen)) {
		vic_hash_append_sg(rctx);

		err = vic_hash_xmit_cpu(hdev, rctx, 0);
		rctx->bufcnt = 0;
	}

	vic_hash_append_sg(rctx);

	if (final) {
		err = vic_hash_xmit_cpu(hdev, rctx,
					(rctx->flags & HASH_FLAGS_FINUP));
		rctx->bufcnt = 0;
	} else {
		err = vic_hash_xmit_cpu(hdev, rctx,
					0);
		rctx->bufcnt = 0;
	}

	return err;
}

static int vic_hash_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(tfm);
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	struct vic_sec_dev *hdev = ctx->sdev;

	memset(rctx,0,sizeof(struct vic_sec_request_ctx));

	rctx->sdev = hdev;

	rctx->sdev->req = req;

	rctx->digcnt = crypto_ahash_digestsize(tfm);
	switch (rctx->digcnt) {
	case MD5_DIGEST_SIZE:
		rctx->mode = SHA_MODE_MD5;
		break;
	case SHA1_DIGEST_SIZE:
		rctx->mode = SHA_MODE_1;
		break;
	case SHA224_DIGEST_SIZE:
		rctx->mode = SHA_MODE_224;
		break;
	case SHA256_DIGEST_SIZE:
		rctx->mode = SHA_MODE_256;
		break;
	case SHA384_DIGEST_SIZE:
		rctx->mode = SHA_MODE_384;
		break;
	case SHA512_DIGEST_SIZE:
		rctx->mode = SHA_MODE_512;
		break;
	default:
		return -EINVAL;
	}

	rctx->bufcnt = 0;
	rctx->buflen = HASH_BUFLEN;
	rctx->total = 0;
	rctx->msg_tot = 0;
	rctx->offset = 0;
	rctx->is_load = 0;
	rctx->last_block_idx = 0;

	memset(rctx->buffer, 0, HASH_BUFLEN);

	if (ctx->flags & HASH_FLAGS_HMAC)
		rctx->flags |= HASH_FLAGS_HMAC;

	dev_dbg(hdev->dev, "%s Flags %lx\n", __func__, rctx->flags);

	return 0;
}

static int vic_hash_update_req(struct vic_sec_dev *hdev)
{
	return vic_hash_update_cpu(hdev);
}

static int vic_hash_final_req(struct vic_sec_dev *hdev)
{
	struct ahash_request *req = hdev->req;
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	int err;

	err = vic_hash_xmit_cpu(hdev, rctx, 1);

	rctx->bufcnt = 0;


	return err;
}

static void vic_hash_set_ctx(struct ahash_request *req)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);

	vic_write_n(rctx->sdev->io_base + SHA_CTX_RAM_OFFSET,
		   rctx->digest, CTX_BLOCK_SIZE);
}

static void vic_hash_copy_hash(struct ahash_request *req)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	unsigned int hashsize;

	switch (rctx->mode) {
	case SHA_MODE_MD5:
		hashsize = MD5_DIGEST_SIZE;
		break;
	case SHA_MODE_1:
		hashsize = SHA1_DIGEST_SIZE;
		break;
	case SHA_MODE_224:
		hashsize = SHA224_DIGEST_SIZE;
		break;
	case SHA_MODE_256:
		hashsize = SHA256_DIGEST_SIZE;
		break;
	case SHA_MODE_384:
		hashsize = SHA384_DIGEST_SIZE;
		break;
	case SHA_MODE_512:
		hashsize = SHA512_DIGEST_SIZE;
		break;
	default:
		return;
	}

	vic_read_n(rctx->sdev->io_base + SHA_MSG_RAM_OFFSET + rctx->sha_cfg.sha_ctx_msg_addr,
		   rctx->digest, hashsize);
}

static int vic_hash_finish(struct ahash_request *req)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);

	if (!req->result)
		return -EINVAL;

	memcpy(req->result, rctx->digest, rctx->digcnt);

	return 0;
}

static void vic_hash_finish_req(struct ahash_request *req, int err)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	struct vic_sec_dev *hdev = rctx->sdev;

	if (!err && (HASH_FLAGS_FINAL & hdev->flags)) {
		vic_hash_copy_hash(req);
		err = vic_hash_finish(req);
		hdev->flags &= ~(HASH_FLAGS_FINAL |
				 HASH_FLAGS_INIT | HASH_FLAGS_HMAC);
	} else {
		rctx->flags |= HASH_FLAGS_ERRORS;
	}

	crypto_finalize_hash_request(hdev->engine, req, err);
}

static int vic_hash_one_request(struct crypto_engine *engine, void *areq);
static int vic_hash_prepare_req(struct crypto_engine *engine, void *areq);

static int vic_hash_handle_queue(struct vic_sec_dev *hdev,
				   struct ahash_request *req)
{
	return crypto_transfer_hash_request_to_engine(hdev->engine, req);
}

static int vic_hash_prepare_req(struct crypto_engine *engine, void *areq)
{
	struct ahash_request *req = container_of(areq, struct ahash_request,
						 base);
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	struct vic_sec_dev *hdev = ctx->sdev;
	struct vic_sec_request_ctx *rctx;

	if (!hdev) {
		return -ENODEV;
	}

	rctx = ahash_request_ctx(req);

	dev_dbg(hdev->dev, "processing new req, op: %lu, nbytes %d\n",
		rctx->op, req->nbytes);

	return 0;
}

static int vic_hash_one_request(struct crypto_engine *engine, void *areq)
{
	struct ahash_request *req = container_of(areq, struct ahash_request,
						 base);
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	struct vic_sec_dev *hdev = ctx->sdev;
	struct vic_sec_request_ctx *rctx;
	int err = 0;

	if (!hdev) {
		return -ENODEV;
	}

	rctx = ahash_request_ctx(req);

	if(rctx->is_load) {
		vic_hash_set_ctx(req);
		rctx->is_load = 0;
	}

	if (rctx->op == HASH_OP_UPDATE){
		err = vic_hash_update_req(hdev);
	} else if (rctx->op == HASH_OP_FINAL) {
		err = vic_hash_final_req(hdev);
	}

	if (err != -EINPROGRESS)
	/* done task will not finish it, so do it here */
		vic_hash_finish_req(req, err);

	return 0;
}

static int vic_hash_enqueue(struct ahash_request *req, unsigned int op)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	struct vic_sec_ctx *ctx = crypto_tfm_ctx(req->base.tfm);
	struct vic_sec_dev *hdev = ctx->sdev;

	rctx->op = op;

	return vic_hash_handle_queue(hdev, req);
}

static int vic_hash_update(struct ahash_request *req)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);

	if (!req->nbytes)
		return 0;

	rctx->total = req->nbytes;
	rctx->sg = req->src;
	rctx->offset = 0;

	if ((rctx->total >= rctx->buflen) ||
	       (rctx->bufcnt + rctx->total >= rctx->buflen)) {
		if(rctx->is_load) {
			vic_hash_set_ctx(req);
			rctx->is_load = 0;
		}
		while ((rctx->total >= rctx->buflen) ||
		       (rctx->bufcnt + rctx->total >= rctx->buflen)) {
			vic_hash_append_sg(rctx);

			vic_hash_xmit_cpu(rctx->sdev, rctx, 0);
			rctx->bufcnt = 0;
		}
		rctx->is_load = 1;
		vic_read_n(rctx->sdev->io_base + SHA_CTX_RAM_OFFSET,
			   rctx->digest, CTX_BLOCK_SIZE);
	}

	if ((rctx->bufcnt + rctx->total < rctx->buflen)) {
		vic_hash_append_sg(rctx);
		return 0;
	}


	return vic_hash_enqueue(req, HASH_OP_UPDATE);
}

static int vic_hash_final(struct ahash_request *req)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);

	rctx->flags |= HASH_FLAGS_FINUP;

	return vic_hash_enqueue(req, HASH_OP_FINAL);
}

static int vic_hash_finup(struct ahash_request *req)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);
	int err1, err2;

	rctx->flags |= HASH_FLAGS_FINUP;

	err1 = vic_hash_update(req);

	if (err1 == -EINPROGRESS || err1 == -EBUSY) {
		return err1;
	}

	/*
	 * final() has to be always called to cleanup resources
	 * even if update() failed, except EINPROGRESS
	 */
	err2 = vic_hash_final(req);

	return err1 ?: err2;
}

static int vic_hash_digest(struct ahash_request *req)
{
	return vic_hash_init(req) ?: vic_hash_finup(req);
}

static int vic_hash_export(struct ahash_request *req, void *out)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);

	memcpy(out, rctx, sizeof(*rctx));

	return 0;
}

static int vic_hash_import(struct ahash_request *req, const void *in)
{
	struct vic_sec_request_ctx *rctx = ahash_request_ctx(req);

	memcpy(rctx, in, sizeof(*rctx));

	return 0;
}
#if 0
static int vic_hash224_setkey(struct crypto_ahash *tfm,
			     const u8 *key, unsigned int keylen)
{
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(tfm);

	if (keylen <= SHA224_BLOCK_SIZE) {
		memcpy(ctx->key, key, keylen);
		ctx->keylen = keylen;
	} else {
		return -ENOMEM;
	}

	return 0;
}
#endif
#if 0
static int vic_hash256_setkey(struct crypto_ahash *tfm,
			     const u8 *key, unsigned int keylen)
{
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(tfm);

	if (keylen <= SHA256_BLOCK_SIZE) {
		memcpy(ctx->key, key, keylen);
		ctx->keylen = keylen;
	} else {
		return -ENOMEM;
	}

	return 0;
}

static int vic_hash384_setkey(struct crypto_ahash *tfm,
			     const u8 *key, unsigned int keylen)
{
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(tfm);

	if (keylen <= SHA384_BLOCK_SIZE) {
		memcpy(ctx->key, key, keylen);
		ctx->keylen = keylen;
	} else {
		return -ENOMEM;
	}

	return 0;
}
#else
static int vic_hmac_setkey(struct crypto_ahash *tfm,
			     const u8 *key, unsigned int keylen)
{
	unsigned int digestsize = crypto_ahash_digestsize(tfm);
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(tfm);
	struct crypto_wait wait;
	struct ahash_request *req;
	struct scatterlist sg;
	unsigned int blocksize;
	struct crypto_ahash *ahash_tfm;
	u8 *buf;
	int ret;
	const char *alg_name;

	blocksize = crypto_tfm_alg_blocksize(crypto_ahash_tfm(tfm));

	if (keylen <= blocksize) {
		memcpy(ctx->key, key, keylen);
		ctx->keylen = keylen;
		return 0;
	}

	if (digestsize == SHA256_DIGEST_SIZE)
		alg_name = "vic-sha256";
	else if (digestsize == SHA384_DIGEST_SIZE)
		alg_name = "vic-sha384";
	else
		return -EINVAL;

	ctx->keylen = digestsize;
	ahash_tfm = crypto_alloc_ahash(alg_name, 0, 0);
	if (IS_ERR(ahash_tfm))
		return PTR_ERR(ahash_tfm);

	req = ahash_request_alloc(ahash_tfm, GFP_KERNEL);
	if (!req) {
		ret = -ENOMEM;
		goto err_free_ahash;
	}

	crypto_init_wait(&wait);
	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				   crypto_req_done, &wait);
	crypto_ahash_clear_flags(ahash_tfm, ~0);

	buf = kzalloc(keylen + VIC_MAX_ALIGN_SIZE, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto err_free_req;
	}

	memcpy(buf, key, keylen);
	sg_init_one(&sg, buf, keylen);
	ahash_request_set_crypt(req, &sg, ctx->key, keylen);

	ret = crypto_wait_req(crypto_ahash_digest(req), &wait);

err_free_req:
	ahash_request_free(req);
err_free_ahash:
	crypto_free_ahash(ahash_tfm);
	return ret;
}
#endif

#if 0
static int vic_hash512_setkey(struct crypto_ahash *tfm,
			     const u8 *key, unsigned int keylen)
{
	struct vic_sec_ctx *ctx = crypto_ahash_ctx(tfm);

	if (keylen <= SHA512_BLOCK_SIZE) {
		memcpy(ctx->key, key, keylen);
		ctx->keylen = keylen;
	} else {
		return -ENOMEM;
	}

	return 0;
}
#endif
static int vic_hash_cra_init_algs(struct crypto_tfm *tfm,
				    const char *algs_hmac_name)
{
	struct vic_sec_ctx *ctx = crypto_tfm_ctx(tfm);

	ctx->sdev = vic_sec_find_dev(ctx);

	if (!ctx->sdev)
		return -ENODEV;

	mutex_lock(&ctx->sdev->lock);

	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct vic_sec_request_ctx));

	ctx->keylen = 0;

	if (algs_hmac_name)
		ctx->flags |= HASH_FLAGS_HMAC;

	ctx->enginectx.op.do_one_request = vic_hash_one_request;
	ctx->enginectx.op.prepare_request = vic_hash_prepare_req;
	ctx->enginectx.op.unprepare_request = NULL;
	return 0;
}

static int vic_hash_cra_init(struct crypto_tfm *tfm)
{
	return vic_hash_cra_init_algs(tfm, NULL);
}

static void vic_hash_cra_exit(struct crypto_tfm *tfm)
{
	struct vic_sec_ctx *ctx = crypto_tfm_ctx(tfm);

	mutex_unlock(&ctx->sdev->lock);
}
#if 0
static int vic_hash_cra_md5_init(struct crypto_tfm *tfm)
{
	return vic_hash_cra_init_algs(tfm, "md5");
}

static int vic_hash_cra_sha1_init(struct crypto_tfm *tfm)
{
	return vic_hash_cra_init_algs(tfm, "sha1");
}

static int vic_hash_cra_sha224_init(struct crypto_tfm *tfm)
{
	return vic_hash_cra_init_algs(tfm, "sha224");
}
#endif
static int vic_hash_cra_sha256_init(struct crypto_tfm *tfm)
{
	return vic_hash_cra_init_algs(tfm, "sha256");
}

static int vic_hash_cra_sha384_init(struct crypto_tfm *tfm)
{
	return vic_hash_cra_init_algs(tfm, "sha384");
}
#if 0
static int vic_hash_cra_sha512_init(struct crypto_tfm *tfm)
{
	return vic_hash_cra_init_algs(tfm, "sha512");
}
#endif
static struct ahash_alg algs_md5_sha512[] = {
#if 0
	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.halg = {
			.digestsize = MD5_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "md5",
				.cra_driver_name = "vic-md5",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = MD5_HMAC_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.setkey = vic_hash_setkey,
		.halg = {
			.digestsize = MD5_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "hmac(md5)",
				.cra_driver_name = "vic-hmac-md5",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = MD5_HMAC_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_md5_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.halg = {
			.digestsize = SHA1_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "sha1",
				.cra_driver_name = "vic-sha1",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA1_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.setkey = vic_hash_setkey,
		.halg = {
			.digestsize = SHA1_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "hmac(sha1)",
				.cra_driver_name = "vic-hmac-sha1",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA1_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_sha1_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},

	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.halg = {
			.digestsize = SHA224_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "sha224",
				.cra_driver_name = "vic-sha224",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA224_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.setkey = vic_hash224_setkey,
		.halg = {
			.digestsize = SHA224_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "hmac(sha224)",
				.cra_driver_name = "vic-hmac-sha224",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA224_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_sha224_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
#endif
	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.halg = {
			.digestsize = SHA256_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "sha256",
				.cra_driver_name = "vic-sha256",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA256_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.setkey = vic_hmac_setkey,
		.halg = {
			.digestsize = SHA256_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "hmac(sha256)",
				.cra_driver_name = "vic-hmac-sha256",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA256_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_sha256_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.halg = {
			.digestsize = SHA384_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "sha384",
				.cra_driver_name = "vic-sha384",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA384_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.setkey = vic_hmac_setkey,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.halg = {
			.digestsize = SHA384_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "hmac(sha384)",
				.cra_driver_name = "vic-hmac-sha384",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
				             CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA384_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_sha384_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
#if 0
	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.halg = {
			.digestsize = SHA512_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "sha512",
				.cra_driver_name = "vic-sha512",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA512_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
	{
		.init = vic_hash_init,
		.update = vic_hash_update,
		.final = vic_hash_final,
		.finup = vic_hash_finup,
		.digest = vic_hash_digest,
		.setkey = vic_hash512_setkey,
		.export = vic_hash_export,
		.import = vic_hash_import,
		.halg = {
			.digestsize = SHA512_DIGEST_SIZE,
			.statesize = sizeof(struct vic_sec_request_ctx),
			.base = {
				.cra_name = "hmac(sha512)",
				.cra_driver_name = "vic-hmac-sha512",
				.cra_priority = 200,
				.cra_flags = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_TYPE_AHASH,
				.cra_blocksize = SHA512_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct vic_sec_ctx),
				.cra_alignmask = 3,
				.cra_init = vic_hash_cra_sha512_init,
				.cra_exit = vic_hash_cra_exit,
				.cra_module = THIS_MODULE,
			}
		}
	},
#endif
};

int vic_hash_register_algs()
{
	int ret = 0;

	ret = crypto_register_ahashes(algs_md5_sha512, ARRAY_SIZE(algs_md5_sha512));

	return ret;
}

int vic_hash_unregister_algs()
{
	crypto_unregister_ahashes(algs_md5_sha512, ARRAY_SIZE(algs_md5_sha512));
	return 0;
}
