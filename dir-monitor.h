#ifndef DIR_MONITOR_H_INCLUDED
#define DIR_MONITOR_H_INCLUDED

struct dir_monitor;
struct dir_monitor_list;

int dir_monitor_start(struct dir_monitor **out, const char *dirpath);

void dir_monitor_stop(struct dir_monitor *dm);

struct dir_monitor_list *dir_monitor_list_create(void);

int dir_monitor_list_add(struct dir_monitor_list *dm_list,
			 const char *dirpath);

int dir_monitor_list_remove(struct dir_monitor_list *dm_list,
			    const char *dirpath);

void dir_monitor_list_destroy(struct dir_monitor_list *dm_list);

#endif /* DIR_MONITOR_H_INCLUDED */
