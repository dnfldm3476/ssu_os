#include <device/io.h>
#include <mem/mm.h>
#include <mem/paging.h>
#include <device/console.h>
#include <proc/proc.h>
#include <interrupt.h>
#include <mem/palloc.h>
#include <ssulib.h>

uint32_t *PID0_PAGE_DIR;

intr_handler_func pf_handler;

uint32_t scale_up(uint32_t base, uint32_t size)
{
	uint32_t mask = ~(size-1);
	if(base & mask != 0)
		base = base & mask + size;
	return base;
}

uint32_t scale_down(uint32_t base, uint32_t size)
{
	uint32_t mask = ~(size-1);
	if(base & mask != 0)
		base = base & mask - size;
	return base;
}
void init_paging()
{
	uint32_t *page_dir = palloc_get_page();
	uint32_t *page_tbl = palloc_get_page();
	page_dir = VH_TO_RH(page_dir); // 0x200000
	page_tbl = VH_TO_RH(page_tbl); // 0x201000 
	PID0_PAGE_DIR = page_dir;

	int NUM_PT, NUM_PE;
	uint32_t cr0_paging_set;
	int i;

	NUM_PE = mem_size() / PAGE_SIZE;
	NUM_PT = NUM_PE / 1024;

	printk("-PE=%d, PT=%d\n", NUM_PE, NUM_PT);
	printk("-page dir=%x page tbl=%x\n", page_dir, page_tbl);
	//페이지 디렉토리 구성
	page_dir[0] = (uint32_t)page_tbl | PAGE_FLAG_RW | PAGE_FLAG_PRESENT;
	NUM_PE = RKERNEL_HEAP_START / PAGE_SIZE; // 512
	//페이지 테이블 구성
	for ( i = 0; i < NUM_PE; i++ ) {
		page_tbl[i] = (PAGE_SIZE * i) // 0x1000씩 떨어진곳
			| PAGE_FLAG_RW
			| PAGE_FLAG_PRESENT;
		//writable & present
	}

	//CR0레지스터 설정
	cr0_paging_set = read_cr0() | CR0_FLAG_PG;		// PG bit set

	//컨트롤 레지스터 저장
	write_cr3( (unsigned)page_dir );		// cr3 레지스터에 PDE 시작주소 저장
	write_cr0( cr0_paging_set );          // PG bit를 설정한 값을 cr0 레지스터에 저장

	reg_handler(14, pf_handler);
}

void memcpy_hard(void* from, void* to, uint32_t len)
{
	write_cr0( read_cr0() & ~CR0_FLAG_PG);
	memcpy(from, to, len);
	write_cr0( read_cr0() | CR0_FLAG_PG);
}

uint32_t* get_cur_pd()
{
	return (uint32_t*)read_cr3();
}

uint32_t pde_idx_addr(uint32_t* addr)
{
	uint32_t ret = ((uint32_t)addr & PAGE_MASK_PDE) >> PAGE_OFFSET_PDE;
	return ret;
}

uint32_t pte_idx_addr(uint32_t* addr) // 
{
	uint32_t ret = ((uint32_t)addr & PAGE_MASK_PTE) >> PAGE_OFFSET_PTE;
	return ret;
}

uint32_t* pt_pde(uint32_t pde)   // page_table의 가상 주소
{
	uint32_t * ret = (uint32_t*)(pde & PAGE_MASK_BASE);
	return ret;
}

uint32_t* pt_addr(uint32_t* addr) // table
{
	uint32_t idx = pde_idx_addr(addr);
	uint32_t* pd = get_cur_pd();
	return pt_pde(pd[idx]);
}

uint32_t* pg_addr(uint32_t* addr) // page 주소 얻기
{
	uint32_t *pt = pt_addr(addr);
	uint32_t idx = pte_idx_addr(addr); // 디렉토리의 몇번째 테이블인지
	uint32_t *ret = (uint32_t*)(pt[idx] & PAGE_MASK_BASE);
	return ret;
}

/*
   page table 복사 , page를 가져와서 집어넣기
 */
void  pt_copy(uint32_t *pd, uint32_t *dest_pd, uint32_t idx)
{
	uint32_t *pt = pt_pde(pd[idx]);
	uint32_t i;
	uint32_t *dest_table = palloc_get_page(); // 디렉토리에 매핑할 테이블 할당
	pt = RH_TO_VH(pt); // source 디렉토리의 엔트리에 담겨진 테이블을 가상주소로 바꾼다.
	dest_pd[idx] = (uint32_t) VH_TO_RH(dest_table) | PAGE_FLAG_RW | PAGE_FLAG_PRESENT;
	// 디렉토리의 엔트리에 테이블의 물리주소를 매핑한다.
	for(i = 0; i<1024; i++)
	{
		if(pt[i] & PAGE_FLAG_PRESENT)
		{ 
			dest_table[i] = pt[i]; // 테이블의 엔트리를 하나씩 복사한다. (물리주소)
		}
	}
}

/*
   page directory 복사. 
 */
void pd_copy(uint32_t* from, uint32_t* to)
{
	uint32_t i;
	for(i = 0; i < 1024; i++) // page table에서 present 설정시 테이블을 복사한다.
	{ 
		if(from[i] & PAGE_FLAG_PRESENT) {//  
			pt_copy(from, to, i);
		}
	} 
}

uint32_t* pd_create (pid_t pid)
{
	uint32_t *pd = palloc_get_page(); // 프로세스 생성시  page table하나 할당

	pd_copy(RH_TO_VH(cur_process -> pd), pd); // 가상주소로 넘겨서 디렉토리 복사를 시작한다.

	pd = VH_TO_RH(pd);  // 실제 주소로 변환

	return pd;
}

void pf_handler(struct intr_frame *iframe)
{
	void *fault_addr;
	asm ("movl %%cr2, %0" : "=r" (fault_addr)); // cr2레지스터로부터 페이지폴트가 일어난 주소를 가져온다.
	printk("Page fault : %X\n", fault_addr);
#ifdef SCREEN_SCROLL
	refreshScreen();
#endif

	uint32_t pt_idx = pde_idx_addr(fault_addr); // 디렉토리의 몇번째 테이블인지.
	uint32_t pg_idx = pte_idx_addr(fault_addr); // 테이블의   몇번째 페이지인지.
	uint32_t *pd = cur_process -> pd;
	if (cur_process -> pid == 0)
		write_cr0( read_cr0() & ~CR0_FLAG_PG); // 페이징 끄기
	else
		pd = RH_TO_VH(pd); // pid가 0이 아니면 페이징으로 메모리에 접근하면서 페이지 폴트를 해결한다.

	if (pd[pt_idx] == 0) {//pd[pt_idx] == 0) { // 만약 page directorty의 페이지 테이블이 비었으면,,,,
		uint32_t *new_table = palloc_get_page();
		uint32_t new_pde_idx = pde_idx_addr(new_table);
		uint32_t new_pte_idx = pte_idx_addr(new_table);

		pd[new_pde_idx] = (uint32_t) VH_TO_RH(new_table) | PAGE_FLAG_RW | PAGE_FLAG_PRESENT; // 물리에 올렷음!
		uint32_t *tmp_table = pt_pde(pd[new_pde_idx]);
		if (cur_process -> pid != 0) // pid가 0이 아니면 가상주소로 접근을 위한 가상주소 전환.
			tmp_table = RH_TO_VH(tmp_table);
		tmp_table[new_pte_idx] = (uint32_t) VH_TO_RH(new_table) | PAGE_FLAG_RW | PAGE_FLAG_PRESENT;
		// table은 페이지가 자기 자신 그래야 fault가 안남. 어차피 자기 그위치는 자기 자신밖에 못가리킴
	}
	uint32_t *page_tbl = pt_pde(pd[pt_idx]); // page table 얻기
	pd[pt_idx] = (uint32_t) page_tbl | PAGE_FLAG_RW | PAGE_FLAG_PRESENT; // 페이지 디렉토리의 엔트리에 테이블 매핑
	if (cur_process -> pid != 0) // 가상주소로 바꾸기
		page_tbl = RH_TO_VH(page_tbl);
	page_tbl[pg_idx] = (uint32_t) VH_TO_RH(fault_addr) | PAGE_FLAG_RW | PAGE_FLAG_PRESENT; // 페이지 테이블의 엔트리 중 하나에  페이지폴트가 일어난 주소를 매핑.

	write_cr0( read_cr0() | CR0_FLAG_PG); // 페이징 온 만약에 켜있으면 어차피 킴

}
