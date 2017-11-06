#include <list.h>
#include <proc/sched.h>
#include <mem/malloc.h>
#include <proc/proc.h>
#include <proc/switch.h>
#include <interrupt.h>

extern struct list plist;
extern struct list rlist;
extern struct process procs[PROC_NUM_MAX];
extern struct list active[PLIST_ROW][PLIST_COL];
extern struct list expired[PLIST_ROW][PLIST_COL];
extern struct list *pr_pool1;
extern struct list *pr_pool2;
bool more_prio(const struct list_elem *a, const struct list_elem *b,void *aux);
struct process* get_next_proc(struct list *rlist_target);
int scheduling;									// interrupt.c
// Browse the 'active' array

struct process* sched_find_set(void) {
	struct process * result = NULL;
	struct list *sched_list = NULL;
	while(result == NULL) {
		for (int i = 0 ; i < PLIST_ROW; i++) { // 비어있지 않은 리스트 찾기
			if (sched_list != NULL)
				break;
			for (int j = 0; j < PLIST_COL; j++) {
				if (list_empty(&pr_pool1[i * PLIST_COL + j])) 
					continue;
				else {
					sched_list = &pr_pool1[i * PLIST_COL + j];
					break;
				}
			}
		}
		if (sched_list == NULL) {  // 리스트가 비어있다면 active와 expired를 교체
			struct list *tmp;
			tmp = pr_pool1;
			pr_pool1 = pr_pool2;
			pr_pool2 = tmp;
		}
		else if (sched_list != NULL) {  // 비어있지 않은 리스트에서 프로세스를 가져옴
			result = get_next_proc(sched_list);
		}
	}

	return result;
}



struct process* get_next_proc(struct list *rlist_target) { // target list의 다음 프로세스를 가져옴.
	struct list_elem *e;
	for(e = list_begin (rlist_target); e != list_end (rlist_target);
			e = list_next (e))
	{	
		struct process* p = list_entry(e, struct process, elem_stat); // process꺼내기
		list_push_back(&pr_pool2[p->priority], &p->elem_stat); // 꺼낸 프로세스를 expired에 집어넣기

		if(p->state == PROC_RUN) {
			return p;
		}
	}
	return NULL;
}


void schedule(void) {
	struct process *cur;
	struct process *next;
	bool first = false;
	if (cur_process -> pid != 0) { // 프로세스를 0번으로 교체
		next = &procs[0];
	}
	else {
		struct list_elem *e;
		for (e = list_begin(&plist); e != list_end(&plist); e = list_next(e)) {
			struct process *p = list_entry(e, struct process, elem_all);
			if (p -> pid == 0) // 0번이면 돌리기
				continue;
			if (p -> state == PROC_RUN) { // 프로세스 상태가 RUN인것을 출력한다.
				if (first == false) {
					printk("#= %d p= %3d c= %2d u= %3d", p -> pid, p-> priority, p -> time_slice,
							p -> time_used);
					first = true;
				}
				else {
					printk(", #= %d p= %3d c= %2d u= %3d", p -> pid, p-> priority, p -> time_slice,
							p -> time_used);
				}
			}
		}
		next = sched_find_set(); // 다음 프로세스를 탐색
		printk("\n");
		printk("Selected : %d\n",  next -> pid);
		proc_wake();
	}
	cur = cur_process;
	cur_process = next; // 현재 프로세스를 다음 프로세스로 집어넣기
	cur_process -> time_slice = 0;

	enum intr_level old_level = intr_disable(); // context switch 간에 인터럽트 발생 방지
	switch_process(cur, next);
	intr_set_level (old_level);
}
