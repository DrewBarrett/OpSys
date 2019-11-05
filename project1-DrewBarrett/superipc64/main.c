#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>

struct messages {
    unsigned char *message;
    long mlength;
    unsigned long key;
    struct list_head messages;
};

struct mbox_421 {
    unsigned long id;
    int enable_crypt;
    struct rb_node node;
    struct messages messages;
    int length;
};

static struct rb_root root = RB_ROOT;
static int rb_size = 0;
static DEFINE_SPINLOCK(lockyboi);

static int my_insert(struct rb_root *root, struct mbox_421 *data)
{
    struct rb_node **new, *parent;
    struct mbox_421 *this;
    new = &(root->rb_node);
    parent = NULL;
    /* Figure out where to put new node */
    while (*new) {
        parent = *new;
        this = rb_entry(parent, struct mbox_421, node);

        if (this->id > data->id)
            new = &((*new)->rb_left);
        else if (this->id < data->id)
            new = &((*new)->rb_right);
        else
            return -17; //EEXISTS
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&data->node, parent, new);
    rb_insert_color(&data->node, root);

    rb_size++;
    return 0;
}

struct mbox_421 *my_search(struct rb_root *root, unsigned long value)
{
    struct rb_node *node = root->rb_node;  /* top of the tree */
    while (node)
    {
        struct mbox_421 *it = rb_entry(node, struct mbox_421, node);
        if (it->id > value)
            node = node->rb_left;
        else if (it->id < value)
            node = node->rb_right;
        else
            return it;  /* Found it */
    }
    return NULL;
}

/******************************************************************************
 * Creates a new empty mailbox with ID id, if it does not already exist, and
 * returns 0. The queue should be flagged for encryption if the enable_crypt
 * option is set to anything other than 0. If enable_crypt is set to zero, then
 * the key parameter in any functions including it should be ignored. Only
 * ROOT can call this. Locks RBTREE to ensure no corruption.
 *****************************************************************************/
SYSCALL_DEFINE2(create_mbox_421, unsigned long, id, int, enable_crypt) {
    int ret;
    struct mbox_421 *thestruct;
    unsigned long flags;
    // Check for root here
    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;
    // Lock RBTREE Here
    spin_lock_irqsave(&lockyboi, flags);
    thestruct = kmalloc(sizeof *thestruct, GFP_KERNEL);
    thestruct->id = id;
    thestruct->enable_crypt = enable_crypt;
    thestruct->length = 0;
    INIT_LIST_HEAD(&thestruct->messages.messages);
    ret = my_insert(&root, thestruct);
    spin_unlock_irqrestore(&lockyboi, flags);
    return ret;
}

/******************************************************************************
 * removes mailbox with ID id, if it is empty, and returns 0. If the mailbox is
 * not empty, this system call should return an appropriate error and not
 * remove the mailbox. Only ROOT can call this. Locks RBTREE to ensure no
 * corruption
 *****************************************************************************/
SYSCALL_DEFINE1(remove_mbox_421, unsigned long, id) {
    unsigned long flags;
    struct mbox_421 *target;
    // Check for root here
    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;
    // Lock RBTREE here
    spin_lock_irqsave(&lockyboi, flags);
    target = my_search(&root, id);
    if (target) {
        if(list_empty(&target->messages.messages)) {
            // Delete
            rb_erase(&target->node, &root);
            kfree(target);
            rb_size--;
            spin_unlock_irqrestore(&lockyboi, flags);
            return 0;
        }
        else {
            // List not empty
            spin_unlock_irqrestore(&lockyboi, flags);
            return -39; // ENOTEMPTY
        }
    }
    spin_unlock_irqrestore(&lockyboi, flags);
    return -2; // ENOENT
}

/******************************************************************************
 * Returns the number of existing mailboxes
 *****************************************************************************/
SYSCALL_DEFINE0(count_mbox_421) {
    return rb_size;
}

/******************************************************************************
 * returns a list of up to k mailbox IDs in the user-space variable mbxes. It
 * returns the number of IDs written successfully to mbxes on success and an
 * appropriate error code on failure.
 *****************************************************************************/
SYSCALL_DEFINE2(list_mbox_421, long __user *, mbxes, long, k) {
    int ntocopy, i;
    struct rb_node *node;
    unsigned long flags;
    long ret[k];
    ntocopy = k;
    if (k < 0)
        return -EFAULT;
    // LOCK RB TREE
    spin_lock_irqsave(&lockyboi, flags);
    if (rb_size < ntocopy) {
        ntocopy = rb_size;
    }
    i = 0;
    if (access_ok(VERIFY_WRITE, mbxes, ntocopy) == 0) {
        // UNLOCK RB TREE
        spin_unlock_irqrestore(&lockyboi, flags);
        return -EFAULT;
    }
    for (node = rb_first(&root); i < ntocopy; node = rb_next(node)) {
        ret[i] = rb_entry(node, struct mbox_421, node)->id;
        i++;
    }
    if (copy_to_user(mbxes, ret, ntocopy) != 0) {
        spin_unlock_irqrestore(&lockyboi, flags);
        return -EFAULT;
    }
    spin_unlock_irqrestore(&lockyboi, flags);
    return ntocopy;
}

/******************************************************************************
 * encrypts the message msg (if appropriate), adding it to the already existing
 * mailbox identified. Returns the number of bytes stored (which should be
 * equal to the message length n) on success, and an appropriate error code on
 * failure. Messages with negative lengths shall be rejected as invalid and
 * cause an appropriate error to be returned, however messages with a length of
 * zero shall be accepted as valid.
 *****************************************************************************/
SYSCALL_DEFINE4(send_msg_421, unsigned long, id, unsigned char __user *, msg,
                long, n, unsigned long, key) {
    struct messages *themessage;
    struct mbox_421 *target;
    unsigned char *message;
    int tmp, i;
    unsigned long flags;
    // Easy pre first step: chgeck length
    printk(KERN_ALERT "send_msg_421 length: %ld", n);
    if (n < 0)
        return -EFAULT;
    // Lock RBTREE
    spin_lock_irqsave(&lockyboi, flags);
    // First things first: check if mailbox exists
    target = my_search(&root, id);
    if (target) {
        // Second up: check if user space pointer is valid
        if ((tmp = access_ok(VERIFY_READ, msg, n)) == 0){
            printk(KERN_ALERT "Verify read from userspace failed! %d", tmp);
            spin_unlock_irqrestore(&lockyboi, flags);
            return -EFAULT;
        }
        // Ok, lets create the message
        message = kmalloc(n, GFP_KERNEL);
        if ((tmp = copy_from_user(message, msg, n)) != 0) {
            printk(KERN_ALERT "Copy data from userspace failed! %d", tmp);
            kfree(message);
            spin_unlock_irqrestore(&lockyboi, flags);
            return -EFAULT;
        }
        // Now we put the message in the mailbox
        themessage = kmalloc(sizeof *themessage, GFP_KERNEL);
        themessage->mlength = n;
        themessage->key = key;
        // do xor if crypt is enabled
        if (target->enable_crypt) {
            // calculate remainder
            int r = n % 4;
            unsigned char fakemsg[n+r];
            for (i = 0; i < n+r; ++i) {
                if (i >= n) {
                    fakemsg[i] = 0;
                }
                else {
                    fakemsg[i] = message[i];
                }
            }
            for (i = 0; i < n+r; i += 4) {
                // create 32 bit long
                unsigned long l = (fakemsg[i] << 24) |
                    (fakemsg[i+1] << 16) |
                    (fakemsg[i+2] << 8) |
                    (fakemsg[i+3]);
                l = l ^ key;
                fakemsg[i] = (0xFF000000 & l) >> 24;
                fakemsg[i+1] = (0x00FF0000 & l) >> 16;
                fakemsg[i+2] = (0x0000FF00 & l) >> 8;
                fakemsg[i+3] = 0x000000FF & l;
            }
            for (i = 0; i < n; ++i) {
                message[i] = fakemsg[i];
            } 
        }
        themessage->message = message;
        list_add_tail(&themessage->messages, &target->messages.messages);
        target->length++;
        spin_unlock_irqrestore(&lockyboi, flags);
        return 0;
    }
    spin_unlock_irqrestore(&lockyboi, flags);
    return -2; // E NO ENT
}

static long getmsg(unsigned long id, unsigned char __user *msg, long n,
        unsigned long key, bool delete) {
    struct mbox_421 *target;
    struct messages *themessage;
    unsigned char modifiedmessage[n];
    int ntocopy, i;
    unsigned long flags;
    if (n < 0)
        return -EFAULT;
    // Lock rb tree
    spin_lock_irqsave(&lockyboi, flags);
    target = my_search(&root, id);
    if(!target) {
        spin_unlock_irqrestore(&lockyboi, flags);
        return -ENOENT;
    }
    if (access_ok(VERIFY_WRITE, msg, n) == 0) {
        spin_unlock_irqrestore(&lockyboi, flags);
        return -EFAULT;
    }
    themessage = list_first_entry(&target->messages.messages, struct messages, messages);
    ntocopy = n;
    if (themessage->mlength < n)
        ntocopy = themessage->mlength;
    if (target->enable_crypt) {
        // calculate remainder
        int r = n % 4;
        unsigned char fakemsg[n+r];
        for (i = 0; i < n+r; ++i) {
            if (i >= n) {
                fakemsg[i] = 0;
            }
            else {
                fakemsg[i] = themessage->message[i];
            }
        }
        for (i = 0; i < n+r; i += 4) {
            // create 32 bit long
            unsigned long l = (fakemsg[i] << 24) |
                (fakemsg[i+1] << 16) |
                (fakemsg[i+2] << 8) |
                (fakemsg[i+3]);
            l = l ^ key;
            fakemsg[i] = (0xFF000000 & l) >> 24;
            fakemsg[i+1] = (0x00FF0000 & l) >> 16;
            fakemsg[i+2] = (0x0000FF00 & l) >> 8;
            fakemsg[i+3] = 0x000000FF & l;
        }
        for (i = 0; i < n; ++i) {
            modifiedmessage[i] = fakemsg[i];
        } 
    }
    else {
       for (i = 0; i < n; ++i) {
            modifiedmessage[i] = themessage->message[i];
        }
    }
    
    if (copy_to_user(msg, modifiedmessage, ntocopy) != 0) {
        spin_unlock_irqrestore(&lockyboi, flags);
        return -EFAULT;
    }
    if (delete) {
        target->length--;
        list_del(&themessage->messages);
        kfree(themessage->message);
        kfree(themessage);
    }
    // Unlock rbtree
    spin_unlock_irqrestore(&lockyboi, flags);
    return ntocopy;
}

/******************************************************************************
 * copies up to n characters from the next message in the mailbox id to the
 * user-space buffer msg, decrypting with the specified key (if appropriate),
 * and removes the entire message from the mailbox (even if only part of the
 * message is copied out). Returns the number of bytes successfully copied
 * (which should be the minimum of the length of the message that is stored and
 * n) on success or an appropriate error code on failure.
 *****************************************************************************/
SYSCALL_DEFINE4(recv_msg_421, unsigned long, id, unsigned char __user *, msg,
                long, n, unsigned long, key) {
    return(getmsg(id, msg, n, key, true));
}

/******************************************************************************
 * same as recv_msg_421 but doesnt remove message
 *****************************************************************************/
SYSCALL_DEFINE4(peek_msg_421, unsigned long, id, unsigned char __user *, msg,
                long, n, unsigned long, key) {
    return(getmsg(id, msg, n, key, false));
}

/******************************************************************************
 * returns the number of messages in the mailbox id on success or an
 * appropriate error code on failure.
 *****************************************************************************/
SYSCALL_DEFINE1(count_msg_421, unsigned long, id) {
    // Lock RBTREE
    struct mbox_421 *target;
    unsigned long flags;
    spin_lock_irqsave(&lockyboi, flags);
    // Check if ID exists
    target = my_search(&root, id);
    if (target) {
        spin_unlock_irqrestore(&lockyboi, flags);
        return target->length;
    }
    spin_unlock_irqrestore(&lockyboi, flags);
    return -2; // ENOENT
}

/******************************************************************************
 * returns the lenth of the next message that would be returned by calling
 * sys_recv_msg_421() with the same id value (that is the number of bytes in
 * the next message in the mailbox). If there are no messages in the mailbox,
 * this should return an appropriate error value.
 *****************************************************************************/
SYSCALL_DEFINE1(len_msg_421, unsigned long, id) {
    struct mbox_421 *target;
    unsigned long flags;
    long ret;
    spin_lock_irqsave(&lockyboi, flags);
    
    target = my_search(&root, id);
    if (!target) {
        spin_unlock_irqrestore(&lockyboi, flags);
        return -ENOENT;
    }
    if (target->length == 0) {
        spin_unlock_irqrestore(&lockyboi, flags);
        return -42; // ENOMSG
    }
    ret = list_first_entry(&target->messages.messages, struct messages, messages)->mlength;
    spin_unlock_irqrestore(&lockyboi, flags);
    return ret;
}

