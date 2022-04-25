#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/kthread.h>
#include <linux/list_sort.h>
#include <linux/tong.h>
#include <linux/sched.h>
#include <linux/sort.h>
#include <linux/types.h>
DECLARE_HASHTABLE(vvtbl,10);
static struct vcpu_io *vcpu_list;
static int booster_pid;
static int control;
int list_table_vcpu_add(int pid1, int pid2, int id, int cpu)
{
	struct vcpu_io *entry;
	entry=(struct vcpu_io*)kmalloc(sizeof(struct vcpu_io),GFP_KERNEL);
	entry->kvm_pid=pid1;
	entry->vcpu_id=id;
	entry->vcpu_pid=pid2;
	entry->eventfd_time=0;
	entry->vcpu_running_at=cpu;
	entry->IRQ_vcpu_pid=0;
	hash_add(vvtbl,&entry->hnode,entry->vcpu_pid);
	list_add(&entry->lnode,&vcpu_list->lnode);
	printk("kvm pid %d vcpu id %d pid %d\n",entry->kvm_pid,entry->vcpu_id,entry->vcpu_pid);
	return 0;
}
EXPORT_SYMBOL(list_table_vcpu_add);

extern int sched_check_task_is_running(struct task_struct *tsk);
extern void sched_force_schedule(struct task_struct *tsk, int clear_flag);
extern void sched_extend_life(struct task_struct *tsk);

void boost_IO_vcpu(int vcpu_pid, int dest_id)
{
	if(vcpu_pid!=booster_pid)
		return;
	struct list_head *pos;
        struct vcpu_io *entry;
        struct task_struct *IO_vcpu;
	int curr_kvm;
        list_for_each(pos,&vcpu_list->lnode)
        {
                entry=list_entry(pos,struct vcpu_io, lnode);
                if(entry->vcpu_pid == vcpu_pid)
                {
                        curr_kvm = entry->kvm_pid;
                }
        }
	list_for_each(pos,&vcpu_list->lnode)
        {
                entry=list_entry(pos,struct vcpu_io, lnode);
                if(entry->kvm_pid == curr_kvm && entry->vcpu_id == dest_id)
                {
                        IO_vcpu = find_get_task_by_vpid(entry->vcpu_pid);
                }
        }

	if(!IO_vcpu)
                return;

        if(sched_check_task_is_running(IO_vcpu)) //if current running is IRQ_vcpu
	{
		sched_extend_life(IO_vcpu);
	}
	else
        {
                sched_force_schedule(IO_vcpu,1);
        }

}
EXPORT_SYMBOL(boost_IO_vcpu);

void boost_IRQ_vcpu(int vcpu_pid)
{
	booster_pid = vcpu_pid;
	//vhost_pid + vcpu_id => vcpu_io
//	printk("%s %d\n",__func__,vcpu_pid);
	struct list_head *pos;
        struct vcpu_io *entry;
	struct task_struct *IRQ_vcpu;
        list_for_each(pos,&vcpu_list->lnode)
        {
                entry=list_entry(pos,struct vcpu_io, lnode);
                if(entry->vcpu_pid == vcpu_pid)
                {	
			IRQ_vcpu = find_get_task_by_vpid(entry->vcpu_pid);
		}
        }
	//current running task
	if(!IRQ_vcpu)
		return;
		
	if(sched_check_task_is_running(IRQ_vcpu)) //if current running is IRQ_vcpu
        {
                sched_extend_life(IRQ_vcpu);
        }
        else
	{
	//	printk("running others\n");
		sched_force_schedule(IRQ_vcpu,0);
	}
	//if vcpu_io is running. curr->sum_exec_runtime = curr->prev_sum_exec_runtime;
	//else
	//vcpu_io -> others vcpu in this pcpu, set_tsk_thread_flag(tsk,TIF_NEED_RESCHED);
	
}
EXPORT_SYMBOL(boost_IRQ_vcpu);
//add vhost pid to each vcpu structure 
/*
int list_table_update_vhost_pid(int vhost_pid, int kvm_pid)
{
        struct list_head *pos;
        struct vcpu_io *entry;
        int vcpu_pid;

        list_for_each(pos,&vcpu_list->lnode)
        {
                entry=list_entry(pos,struct vcpu_io, lnode);
		if(entry->kvm_pid == kvm_pid)
			entry->vhost_pid = vhost_pid;
        }
        return 0;

}
EXPORT_SYMBOL(list_table_update_vhost_pid);
*/
int list_table_vcpu_have(int pid)
{
	struct vcpu_io *cur;
        hash_for_each_possible(vvtbl,cur,hnode,pid)
        {
		if(cur->vcpu_pid == pid)
			return 1;
        }
        return 0;
}
EXPORT_SYMBOL(list_table_vcpu_have);
int vcpu_table_update_io(int pid,int cpu)
{
	struct vcpu_io *cur;
	hash_for_each_possible(vvtbl,cur,hnode,pid)
	{
		cur->eventfd_time++;
		cur->vcpu_running_at=cpu;
	}
	return 0;
}
EXPORT_SYMBOL(vcpu_table_update_io);
/*
int kvm_exit_clean_up(int pid)
{
	struct kvm_io *entry, *node_to_del;
	struct list_head *pos;
	struct vhost_table *cur2, *table_to_del;
	printk("kvm %d del\n",pid);

	list_for_each(pos,&kvm_list->node)
        {
                entry=list_entry(pos,struct kvm_io, node);
		if(entry->kvm_pid == pid)
		{
                	printk("kvm %d vhost %d net_io %lu\n",entry->kvm_pid,entry->vhost_pid,entry->net_io);
			node_to_del=entry;
			break;
        	}
	}

	hash_for_each_possible(vtbl,cur2,node,node_to_del->vhost_pid)
        {
                table_to_del=cur2;
                printk("vhost %d read %lu write %lu\n",cur2->vhost_pid,cur2->net_io_read,cur2->net_io_write);
	}
	list_del(&node_to_del->node);
	hlist_del(&table_to_del->node);
	return 0;
}
EXPORT_SYMBOL(kvm_exit_clean_up);

*/
int vcpu_table_clean_io(int pid)
{
        struct vcpu_io *cur;
        hash_for_each_possible(vvtbl,cur,hnode,pid)
        {
                cur->eventfd_time=0;
        }
        return 0;
}

int vcpu_table_clean_io_all(void)
{
        struct vcpu_io *cur;
	unsigned bkt;
        hash_for_each(vvtbl,bkt,cur,hnode)
        {
		cur->eventfd_time=0;
        }
        return 0;
}



int vcpu_io_comp(void *priv, const struct list_head *_a,const struct list_head *_b)
{
	struct vcpu_io *a=list_entry(_a, struct vcpu_io, lnode);
	struct vcpu_io *b=list_entry(_b, struct vcpu_io, lnode);
	if(a->eventfd_time > b->eventfd_time)
		return -1;
	else if(a->eventfd_time < b->eventfd_time)
		return 1;
	else return 0;
}
static int vcpu_sort(struct seq_file *m, void *v)
{
	struct list_head *pos;
	struct vcpu_io *entry;
	int vcpu_pid;
	list_sort(NULL,&vcpu_list->lnode,vcpu_io_comp);

	list_for_each(pos,&vcpu_list->lnode)
        {
                entry=list_entry(pos,struct vcpu_io, lnode);
                seq_printf(m,"kvm %d vcpu %d vcpu_pid %d pcpu %d io %lu \n",entry->kvm_pid,entry->vcpu_id,entry->vcpu_pid,entry->vcpu_running_at,entry->eventfd_time);
        }
	return 0;

}

static int vcpu_io_sort(struct seq_file *m, void *v)
{
        struct list_head *pos;
        struct vcpu_io *entry;
        int vcpu_pid;
        list_sort(NULL,&vcpu_list->lnode,vcpu_io_comp);

        list_for_each(pos,&vcpu_list->lnode)
        {
                entry=list_entry(pos,struct vcpu_io, lnode);
		if(entry->eventfd_time!=0)
                seq_printf(m,"vcpu %d io %lu \n",entry->vcpu_pid,entry->eventfd_time);
        }
        return 0;

}
static int vcpu_list_show(struct seq_file *m, void *v)
{
	struct list_head *pos;
	struct vcpu_io *entry;
	list_for_each(pos,&vcpu_list->lnode)
        {
                entry=list_entry(pos,struct vcpu_io, lnode);
		seq_printf(m,"kvm %d vcpu %d vcpu_pid %d pcpu %d io %lu \n",entry->kvm_pid,entry->vcpu_id,entry->vcpu_pid,entry->vcpu_running_at,entry->eventfd_time);
        }
        return 0;
}

static int fake1=100;
static int fake2=200;
static int fake3=1;
static int vcpu_fake_add(struct seq_file *m, void *v)
{
	list_table_vcpu_add(fake1, fake2, fake3,4);
	if(fake3)
		vcpu_table_update_io(fake2,4);


	fake1+=42;
	fake2+=79;
	fake3+=1;
	fake3%=4;
	printk("done\n");
        return 0;
}
static int sleep_time=500;
static int vcpu_boosting_worker(void *arg0)

{
	while(!kthread_should_stop())
	{
		if(control)
			vcpu_table_clean_io_all();
		msleep(sleep_time);
	}
	return 0;
}
static int vcpu_boosting(struct seq_file *m, void *v)
{
        if(control) control=0;
        else control =1;
        seq_printf(m,"vcpu boosting %d\n",control);
        return 0;
}

int cfs_boost_flag =0;
int cfs_print_flag =0;

EXPORT_SYMBOL(cfs_boost_flag);
EXPORT_SYMBOL(cfs_print_flag);

static int cfs_boost(struct seq_file *m, void *v)
{
        if(cfs_boost_flag)
                cfs_boost_flag=0;
        else
                cfs_boost_flag=1;
        seq_printf(m, "cfs_boost_flag %d\n",cfs_boost_flag);
        return 0;
}
static int cfs_print(struct seq_file *m, void *v)
{
        if(cfs_print_flag)
                cfs_print_flag=0;
        else
                cfs_print_flag=1;
        seq_printf(m, "cfs_print_flag %d\n",cfs_print_flag);
        return 0;
}

int check_cpu_boosting(int cpu)
{
	struct list_head *pos;
        struct vcpu_io *entry;
        list_for_each(pos,&vcpu_list->lnode)
        {
                entry=list_entry(pos,struct vcpu_io, lnode);
		//if there is IO in 1 sec 
                if(entry->eventfd_time!=0)
			if(entry->vcpu_running_at == cpu)
				return 1;
			
        }
        return 0;

}
EXPORT_SYMBOL(check_cpu_boosting);


int ctx_sw_flag =0;
static int cfs_ctx_sw_flag(struct seq_file *m, void *v)
{
        if(ctx_sw_flag)
                ctx_sw_flag=0;
        else
                ctx_sw_flag=1;
        seq_printf(m, "ctx_sw_flag %d\n",ctx_sw_flag);
        return 0;
}
static unsigned long long record_min =-1; 
static unsigned long long record_max =0;
static unsigned long long record_exit =-1;
static long counter=0;
static unsigned long long total_ctx=0;
static unsigned long long record;
void cfs_record_run(void)
{
	
	if(rdtsc() < record_exit)
		return;
	record = rdtsc() - record_exit;
	if (record_min > record)
		record_min=record;
	if (record_max < record)
		record_max=record;
	total_ctx+=record;
	counter++;
}

static int cfs_ctx_sw_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ctx_sw_flag %d\n",ctx_sw_flag);
	seq_printf(m, "MAX %lld MIN %lld\n",record_max, record_min);
	seq_printf(m, "Count %ld TOTAL %lld\n",counter,total_ctx);
	return 0;
}
static int cfs_ctx_sw_refresh(struct seq_file *m, void *v)
{
	record_min=-1;
	record_max=0;
	record_exit=-1;
	total_ctx=0;
	counter=0;
	return 0;
}


void cfs_record_exit(void)
{
	record_exit=rdtsc();
}
EXPORT_SYMBOL(ctx_sw_flag);
EXPORT_SYMBOL(cfs_record_exit);
EXPORT_SYMBOL(cfs_record_run);



static int __init proc_cmdline_init(void)
{
	struct task_struct *tsk;
	hash_init(vvtbl);
	vcpu_list=(struct vcpu_io*)kmalloc(sizeof(struct vcpu_io),GFP_KERNEL);
	INIT_LIST_HEAD(&vcpu_list->lnode);
	tsk=kthread_run(vcpu_boosting_worker, NULL, "Huawei_new_Boosting");
        if (IS_ERR(tsk)) {
                printk(KERN_ERR "Cannot create KVM_IO, %ld\n", PTR_ERR(tsk));
        }
	proc_create_single("vcpu_list_sort",0,NULL,vcpu_sort);
	proc_create_single("vcpu_fake_add",0,NULL,vcpu_fake_add);
	proc_create_single("vcpu_list_show",0,NULL,vcpu_list_show);
	proc_create_single("vcpu_boosting_control",0,NULL,vcpu_boosting);
	proc_create_single("vcpu_io",0,NULL,vcpu_io_sort);
	proc_create_single("cfs_boost", 0, NULL, cfs_boost);
	proc_create_single("cfs_print", 0, NULL, cfs_print);
	proc_create_single("cfs_ctx_sw", 0, NULL, cfs_ctx_sw_flag);
	proc_create_single("cfs_ctx_show", 0, NULL, cfs_ctx_sw_show);
	proc_create_single("cfs_ctx_refresh", 0, NULL, cfs_ctx_sw_refresh);



        return 0;
}
fs_initcall(proc_cmdline_init);


