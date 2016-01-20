/*********************************
References : HW1
*********************************/

#include "sys_submitjob.h"
#include "common.h"
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/cryptohash.h>
#include <linux/namei.h>
#include <linux/stat.h>
#include <linux/crc32.h>


#define MAX_FILE_LENGTH 1024
#define MD5_KEY_LENGTH 16
#define SHA_KEY_LENGTH 20


int checksum(struct job *in){
	
	unsigned  long crc = crc32(0L, NULL, 0);
	int err = SUCCESS, bytes = 0;
	struct file *filp1;
	void *buffer;
	mm_segment_t oldfs;
	loff_t rpos = 0;
	unsigned long *user = (unsigned long *)in->crc32;
	
	filp1 = filp_open(in->infile, MAY_READ, 0);
    if (!filp1 || IS_ERR(filp1)) {
	printk("[checksum]: wrapfs_read_file err %d\n", (int) PTR_ERR(filp1));
	err=PTR_ERR(filp1);
	goto out;
    }
	
	buffer = kmalloc(MY_BUFFER_SIZE,GFP_KERNEL);
	if (!buffer) {
		printk("[checksum] : kmalloc FAILED");
    	err = -ENOMEM;
		goto out;
  	}
	
	do{
	oldfs = get_fs();
		set_fs(KERNEL_DS);
		bytes=vfs_read(filp1, buffer, PAGE_SIZE, &rpos);
		if(err<0)
		{
			printk("[checksum] : vfs_read FAILED");
			goto out;
		}
		set_fs(oldfs);
		crc = crc32(crc, buffer, bytes);
	}while(bytes>0);
	
	
	err = copy_to_user(user,&crc,sizeof(unsigned long));
	if(err<0){
			printk("[functionality]: copy_to_user failed\n");
			goto out;
		}
	 
	out: 
	if(filp1){
		if (!IS_ERR(filp1)) {
			filp_close(filp1, NULL);
		}
	}	
	return err;
}

int encryptDecrypt(struct job *in){
	int err = SUCCESS;
	struct file *filp1=NULL, *filp2=NULL;
	filp1 = filp_open(in->infile, MAY_READ, 0);
    if (!filp1 || IS_ERR(filp1)) {
	printk("[sys_submitjob]: wrapfs_read_file err %d\n", (int) PTR_ERR(filp1));
	err=PTR_ERR(filp1);
	goto out_ok;
    }

    filp2 = filp_open(in->outfile, MAY_WRITE|O_CREAT, 0);
    if (!filp2 || IS_ERR(filp2)) {
	printk("[sys_submitjob]: wrapfs_write_file err %d\n", (int) PTR_ERR(filp2));
	err=PTR_ERR(filp2);
	goto out_ok;
    }

	if(filp1->f_inode==filp2->f_inode)
	{
		printk("[functionality]: The two files are same\n");
		err=-1;
		goto out_ok;
	}
	err=rwfile(filp1,filp2,in);
	if(err!=0)
	{
		printk("[functionality]: File read write operation failed\n");
		goto out_ok;
	}   
    	
 

 	out_ok:
	if(filp1){
		if (!IS_ERR(filp1)) {
			filp_close(filp1, NULL);
		}
	}	
	if(filp2){
		if (!IS_ERR(filp2)) {
			filp_close(filp2, NULL);
		}
	}	

	return err;
}


void sha(unsigned char *hash, char *plaintext)
{
	struct scatterlist sg;
    struct crypto_hash *tfm;
    struct hash_desc desc;

    memset(hash, 0x00,SHA_KEY_LENGTH);

    tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);

    desc.tfm = tfm;
    desc.flags = 0;

    sg_init_one(&sg, plaintext, 10);
    crypto_hash_init(&desc);

    crypto_hash_update(&desc, &sg, 10);
    crypto_hash_final(&desc, hash);
	crypto_free_hash(tfm);

}

int crypt(void *rbuff,void *wbuff,unsigned int bytes,struct job *in)
{
	struct crypto_blkcipher *tfm=NULL;
    struct blkcipher_desc desc;
	struct scatterlist *src=NULL;
    struct scatterlist *dst=NULL;
	unsigned char *key=in->key;
	int ret=0;
	char *iv ="\x12\x34\x56\x78\x90\xab\xcd\xef\x12\x34\x56\x78\x90\xab\xcd\xef";
	unsigned int ivsize = 0;
	
	src = kmalloc(sizeof(struct scatterlist), __GFP_ZERO|GFP_KERNEL);
    if (!src) {
        printk("[crypt]: failed to alloc src\n");     
        ret= -1;
		goto out;
    }
    dst = kmalloc(sizeof(struct scatterlist), __GFP_ZERO|GFP_KERNEL);
    if (!dst) {
        printk("[crypt]: failed to alloc dst\n"); 
        ret= -1;
		goto out;
    }
	
	tfm = crypto_alloc_blkcipher("ctr(aes)", 0, 0);
	
	 if (IS_ERR(tfm)) {
        printk("[crypt]: failed to load transform for : %ld\n",PTR_ERR(tfm));
		ret=-1;
        goto out;
    }
    desc.tfm = tfm;
    desc.flags = 0;
    
    ret = crypto_blkcipher_setkey(tfm, key, MD5_KEY_LENGTH);
	 if (ret) {
			printk("[crypt]: setkey() failed flagss=%x\n",
            crypto_blkcipher_get_flags(tfm));
			goto out;
    }
	
	ivsize = crypto_blkcipher_ivsize(tfm);
	if (ivsize) {
		if (ivsize != strlen(iv))
			printk("[crypt]: IV length differs from expected length\n");
			crypto_blkcipher_set_iv(tfm, iv, ivsize);
	}
	
	sg_init_table(src, 1);
	sg_init_table(dst, 1);
		
	sg_set_buf(src, rbuff, bytes);
	sg_set_buf(dst, wbuff, bytes);
	
	if(in->job_type==ENCRYPT){
		ret=crypto_blkcipher_encrypt(&desc,dst,src,bytes);
		if(ret<0)
		{
			printk("[crypt]: crypto_blkcipher_encrypt FAILED");
			goto out;
		}
	}
	else{
		ret=crypto_blkcipher_decrypt(&desc,dst,src,bytes);
		if(ret<0)
		{
			printk("[crypt]: crypto_blkcipher_decrypt FAILED");
			goto out;
		}
	}	
			
out:
    if(dst)
		kfree(dst);
	if(src)
		kfree(src);
	if(tfm){ 
		if(!IS_ERR(tfm))
			crypto_free_blkcipher(tfm);
		}

	return ret;
}



int rwfile(struct file *filp1, struct file *filp2, struct job *in)
{
    void *rbuff=NULL, *wbuff=NULL;
	unsigned char *preamble=NULL,*new_preamble=NULL,*key=in->key;
	int err=0;
	unsigned int bytes=MY_BUFFER_SIZE;
	loff_t rpos = 0, wpos=0;
	mm_segment_t oldfs;
	
	
  
	
	rbuff = kmalloc(MY_BUFFER_SIZE,GFP_KERNEL);
	if (!rbuff) {
		printk("[rwfile] : kmalloc FAILED");
    	err = -ENOMEM;
		goto out;
  	}
	wbuff = kmalloc(MY_BUFFER_SIZE,GFP_KERNEL);
	if (!wbuff) {
		printk("[rwfile] : kmalloc FAILED");
    	err = -ENOMEM;
		goto out;
  	}
	preamble = kmalloc(SHA_KEY_LENGTH,GFP_KERNEL);
	if (!preamble) {
		printk("[rwfile] : kmalloc FAILED");
    	err = -ENOMEM;
		goto out;
  	}
	new_preamble = kmalloc(SHA_KEY_LENGTH,GFP_KERNEL);
	if (!new_preamble) {
		printk("[rwfile] : kmalloc FAILED");
    	err = -ENOMEM;
		goto out;
  	}
	
	sha(preamble,key);
	
	if(in->job_type==ENCRYPT)
	{
		//encryption, put the preamble into write file.
		oldfs = get_fs();
		set_fs(KERNEL_DS);
		err=vfs_write(filp2, preamble, SHA_KEY_LENGTH, &wpos);
		if(err<0)
		{
			printk("[rwfile] : vfs_write FAILED");
			goto out;
		}
		set_fs(oldfs);
			
	}
	else
	{
		//decryption, extract new_preamble from read file, compare, if yes, then go ahead else fail here.
		oldfs = get_fs();
		set_fs(KERNEL_DS);
		err=vfs_read(filp1, new_preamble, SHA_KEY_LENGTH, &rpos);
		if(err<0)
		{
			printk("[rwfile] : vfs_read FAILED");
			goto out;
		}
		set_fs(oldfs);
	
		if(0!=memcmp(preamble,new_preamble,SHA_KEY_LENGTH))
		{
			printk("\nwrong decrypt key, DECRYTION FAILED \n");
			err=-1;
			goto out;
		}
	}
	
	 do{	
	oldfs = get_fs();
    set_fs(KERNEL_DS);
	bytes=vfs_read(filp1, rbuff, PAGE_SIZE, &rpos);
	set_fs(oldfs);

	if(bytes<0)
	{
		printk("[rwfile]: vfs_read FAILED\n");
		err=bytes;
		goto out;
	}
	
	err=crypt(rbuff,wbuff,bytes,in);
	if(err<0)
	{
		printk("[rwfile]: crypt FAILED\n");
		goto out;
	}
	
	oldfs = get_fs();
    set_fs(KERNEL_DS);
	err=vfs_write(filp2, wbuff, bytes, &wpos);
	if(bytes<0)
	{
		printk("[rwfile]: vfs_write FAILED\n");
		goto out;
	}
	set_fs(oldfs);
	
	}while(bytes>0);
	

out:
	
	if(new_preamble)
		kfree(new_preamble);
	if(preamble)
		kfree(preamble);
	if(wbuff)
		kfree(wbuff);
	if(rbuff)
		kfree(rbuff);
	

	return err;
}

