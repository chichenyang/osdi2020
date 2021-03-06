#include "mm.h" `
#include "buddy.h"

#define offsetof(TYPE, MEMBER) ((unsigned int) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
		(type *)( (char *)__mptr - offsetof(type,member) );})


#define list_entry(ptr,type,member)	\
    container_of(ptr, type, member)



#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)


struct list_head {
	struct list_head *next, *prev;
};


static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void __list_add(struct list_head *new_lst,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new_lst;
	new_lst->next = next;
	new_lst->prev = prev;
	prev->next = new_lst;
}

static inline void list_add(struct list_head *new_lst, struct list_head *head)
{
	__list_add(new_lst, head, head->next);
}

static inline void list_add_tail(struct list_head *new_lst, struct list_head *head)
{
	__list_add(new_lst, head->prev, head);
}

static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_del(struct list_head * entry)
{
	__list_del(entry->prev,entry->next);
}


static inline void list_remove_chain(struct list_head *ch,struct list_head *ct){
	ch->prev->next=ct->next;
	ct->next->prev=ch->prev;
}

static inline void list_add_chain(struct list_head *ch,struct list_head *ct,struct list_head *head){
		ch->prev=head;
		ct->next=head->next;
		head->next->prev=ct;
		head->next=ch;
}

static inline void list_add_chain_tail(struct list_head *ch,struct list_head *ct,struct list_head *head){
		ch->prev=head->prev;
		head->prev->next=ch;
		head->prev=ct;
		ct->next=head;
}

static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}


/*init list heads*/
struct list_head page_buddy[MAX_BUDDY_PAGE_NUM];

void init_page_buddy(void){
	int i;
    uart_puts("Init_page_buddy\n");
	for(i=0;i<MAX_BUDDY_PAGE_NUM;i++){
		INIT_LIST_HEAD(&page_buddy[i]);
	}
}

struct page {
	unsigned long addr;
	unsigned int flags;
	int order;
    unsigned int number;
	struct list_head list;//to string the buddy member
};

void init_page_map(void){
	int i;
	struct page *pg=(struct page *)KERNEL_PAGE_START;
    uart_puts("Init_page_map \n");

    uart_puts("KERNE_FROM: ");
    uart_hex(KERNEL_PAGING_START);
    uart_puts(" to ");
    uart_hex(KERNEL_PAGING_END);
    uart_puts("\n");

    uart_puts("KERNEL_PAGE_STRUCT start: ");
    uart_hex(KERNEL_PAGE_START);
    uart_puts("\n");

	init_page_buddy();
	for(i=0;i<(KERNEL_PAGE_NUM);pg++,i++){
        /*fill struct page first*/
		pg->addr=KERNEL_PAGING_START+i*PAGE_SIZE;	
		pg->flags=PAGE_AVAILABLE;
        pg->number=i;
		INIT_LIST_HEAD(&(pg->list));


        /*make the memory max buddy as possible*/
		if(i<(KERNEL_PAGE_NUM&(~PAGE_NUM_FOR_MAX_BUDDY))){	
			if((i&PAGE_NUM_FOR_MAX_BUDDY)==0){
				pg->order=MAX_BUDDY_PAGE_NUM-1;
			}else{
				pg->order=-1;
			}
			list_add_tail(&(pg->list),&page_buddy[MAX_BUDDY_PAGE_NUM-1]);
		}else{
			pg->order=0;
			list_add_tail(&(pg->list),&page_buddy[0]);
		}

	}
   //print_buddy_status();
}


struct page *get_pages_from_list(int order){
	unsigned int vaddr;
	int neworder=order;
	struct page *pg,*ret;
	struct list_head *tlst,*tlst1;

    //print_buddy_status();
    uart_puts("[Required order:");
    uart_hex(neworder);
    uart_puts("]\n");

	for(;neworder<MAX_BUDDY_PAGE_NUM;neworder++){
		if(list_empty(&page_buddy[neworder])){
			continue;
		}else{
			pg=list_entry(page_buddy[neworder].next,struct page,list);
			tlst=&(BUDDY_END(pg,neworder)->list);
			tlst->next->prev=&page_buddy[neworder];
			page_buddy[neworder].next=tlst->next;
			goto OUT_OK;
		}
	}
	return NULL;
OUT_OK:
	for(neworder--;neworder>=order;neworder--){
        if(neworder != order){
	        uart_puts("Cut off the bottom half the block from ");
            uart_hex(neworder);
            uart_puts(" to ");
            uart_hex(neworder-1);
            uart_puts("\n");
        }
    	tlst1=&(BUDDY_END(pg,neworder)->list);
		tlst=&(pg->list);

		pg=NEXT_BUDDY_START(pg,neworder);
		list_entry(tlst,struct page,list)->order=neworder;

		list_add_chain_tail(tlst,tlst1,&page_buddy[neworder]);
	}

    //print_buddy_status();
    uart_getc();

	pg->flags|=PAGE_BUDDY_BUSY;
	pg->order=order;
	return pg;
}

void print_buddy_status(){
    int i = 0;
    uart_puts("**************** BUDDY STATUS *************** \n");
    for(; i < MAX_BUDDY_PAGE_NUM; i++){
        uart_hex(i);
        uart_puts("th order: ");
        if(list_empty(&page_buddy[i])){
            uart_puts("empty");
        }else{
            struct list_head *ptr = page_buddy[i].next;
            struct page *pg;
            int j = 0;
            do{
                pg = list_entry(ptr, struct page, list);
                if(pg->order != -1){
                    uart_hex((unsigned long)pg->number);
//                    uart_hex((unsigned long)pg->addr);                    
                    uart_puts(" -> ");
                    j++;
                }
                ptr = ptr->next;
            } while(j < 10 && ptr != &page_buddy[i]);
        }
        uart_puts("\n");
    }
    uart_puts("******************************************** \n");
}

void put_pages_to_list(struct page *pg,int order){
    
    //print_buddy_status();
    uart_puts("[Put_pages_to_list: page number ");
    uart_hex(pg->number);
    uart_puts(" in order ");
    uart_hex(order);
    uart_puts("]\n");

	struct page *tprev,*tnext;
	if(!(pg->flags&PAGE_BUDDY_BUSY)){
		uart_puts("error: page was not allocated\n");
		return;
	}
	pg->flags&=~(PAGE_BUDDY_BUSY);
	for(;order<MAX_BUDDY_PAGE_NUM;order++){
		tnext=NEXT_BUDDY_START(pg,order);
		tprev=PREV_BUDDY_START(pg,order);
		if((!(tnext->flags&PAGE_BUDDY_BUSY))&&(tnext->order==order)){
            uart_puts("Merge ");
            uart_hex(pg->number);
            uart_puts(" with ");
            uart_hex(tnext->number);
            uart_puts("\n");
		
        	pg->order++;
			tnext->order=-1;
			list_remove_chain(&(tnext->list),&(BUDDY_END(tnext,order)->list));
			BUDDY_END(pg,order)->list.next=&(tnext->list);
			tnext->list.prev=&(BUDDY_END(pg,order)->list);
         
            if(order+1 == MAX_BUDDY_PAGE_NUM)
                 break;
			else
                continue;
		}else if((!(tprev->flags&PAGE_BUDDY_BUSY))&&(tprev->order==order)){
            uart_puts("Merge ");
            uart_hex(pg->number);
            uart_puts(" with ");
            uart_hex(tprev->number);
            uart_puts("\n");
			
            pg->order=-1;			
			list_remove_chain(&(tprev->list),&(BUDDY_END(tprev,order)->list));
			BUDDY_END(tprev,order)->list.next=&(pg->list);
			pg->list.prev=&(BUDDY_END(tprev,order)->list);
			pg=tprev;
			pg->order++; 
            
            if(order+1 == MAX_BUDDY_PAGE_NUM)
                 break;
            else
			    continue;
		}else{
			break;
		}
	}
	list_add_chain(&(pg->list),&(BUDDY_END(pg,order)->list),&page_buddy[order]);
    //print_buddy_status();
	uart_getc();
}


struct page *alloc_pages(int order){
	struct page *pg;
	int i;
	pg=get_pages_from_list(order);
	if(pg==NULL)
		return NULL;
	for(i=0;i<(1<<order);i++){
		(pg+i)->flags|=PAGE_DIRTY;
	}
	return pg;
}

unsigned long page_address(struct page *pg){
	return pg->addr;
}

unsigned long get_free_page(int order){
	struct page *pg;
	pg = alloc_pages(order);
	if (!pg)
		return NULL;
	return	page_address(pg);
}

void free_page(unsigned long p){
    struct page *pg = (struct page*)KERNEL_PAGE_START + ((p - KERNEL_PAGING_START) / (PAGE_SIZE));
    int order = pg->order;
	int i;
	for(i=0;i<(1<<order);i++){
		(pg+i)->flags&=~PAGE_DIRTY;
	}
	put_pages_to_list(pg, order);
}


unsigned long kmalloc(unsigned long size){
    int order;
    for(int i = 0; i < MAX_BUDDY_PAGE_NUM; i++){
        if(size <= (unsigned long)(1<<i)*PAGE_SIZE){
            order = i;
            break;
        }
    }
    return get_free_page(order);
}

void kfree(unsigned long p){
    free_page(p);
}
