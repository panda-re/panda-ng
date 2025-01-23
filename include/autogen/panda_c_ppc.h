       
       
typedef __int128_t Int128;
typedef __int128_t __attribute__((aligned(16))) Int128Aligned;
typedef union {
    __uint128_t u;
    __int128_t i;
    Int128 s;
} Int128Alias __attribute__((transparent_union));
void urshift(uint64_t *plow, uint64_t *phigh, int32_t shift);
void ulshift(uint64_t *plow, uint64_t *phigh, int32_t shift, 
                                                            _Bool 
                                                                 *overflow);
Int128 divu256(Int128 *plow, Int128 *phigh, Int128 divisor);
Int128 divs256(Int128 *plow, Int128 *phigh, Int128 divisor);
       
       
void os_set_line_buffering(void);
void os_setup_early_signal_handling(void);
void os_set_proc_name(const char *s);
void os_setup_signal_handling(void);
int os_set_daemonize(
                    _Bool 
                         d);
_Bool 
    is_daemonized(void);
void os_daemonize(void);
_Bool 
    os_set_runas(const char *user_id);
void os_set_chroot(const char *path);
void os_setup_limits(void);
void os_setup_post(void);
int os_mlock(void);
void *qemu_alloc_stack(size_t *sz);
void qemu_free_stack(void *stack, size_t sz);
typedef struct AccelCPUState AccelCPUState;
typedef struct AccelState AccelState;
typedef struct AddressSpace AddressSpace;
typedef struct AioContext AioContext;
typedef struct Aml Aml;
typedef struct ArchCPU ArchCPU;
typedef struct BdrvDirtyBitmap BdrvDirtyBitmap;
typedef struct BdrvDirtyBitmapIter BdrvDirtyBitmapIter;
typedef struct BlockBackend BlockBackend;
typedef struct BlockBackendRootState BlockBackendRootState;
typedef struct BlockDriverState BlockDriverState;
typedef struct BusClass BusClass;
typedef struct BusState BusState;
typedef struct Chardev Chardev;
typedef struct Clock Clock;
typedef struct ConfidentialGuestSupport ConfidentialGuestSupport;
typedef struct CPUArchState CPUArchState;
typedef struct CPUPluginState CPUPluginState;
typedef struct CPUState CPUState;
typedef struct CPUTLBEntryFull CPUTLBEntryFull;
typedef struct DeviceState DeviceState;
typedef struct DirtyBitmapSnapshot DirtyBitmapSnapshot;
typedef struct DisasContextBase DisasContextBase;
typedef struct DisplayChangeListener DisplayChangeListener;
typedef struct DriveInfo DriveInfo;
typedef struct DumpState DumpState;
typedef struct Error Error;
typedef struct EventNotifier EventNotifier;
typedef struct FlatView FlatView;
typedef struct FWCfgState FWCfgState;
typedef struct HostMemoryBackend HostMemoryBackend;
typedef struct I2CBus I2CBus;
typedef struct I2SCodec I2SCodec;
typedef struct IOMMUMemoryRegion IOMMUMemoryRegion;
typedef struct ISABus ISABus;
typedef struct ISADevice ISADevice;
typedef struct IsaDma IsaDma;
typedef struct JSONWriter JSONWriter;
typedef struct MACAddr MACAddr;
typedef struct MachineClass MachineClass;
typedef struct MachineState MachineState;
typedef struct MemoryListener MemoryListener;
typedef struct MemoryMappingList MemoryMappingList;
typedef struct MemoryRegion MemoryRegion;
typedef struct MemoryRegionCache MemoryRegionCache;
typedef struct MemoryRegionSection MemoryRegionSection;
typedef struct MigrationIncomingState MigrationIncomingState;
typedef struct MigrationState MigrationState;
typedef struct Monitor Monitor;
typedef struct MSIMessage MSIMessage;
typedef struct NetClientState NetClientState;
typedef struct NetFilterState NetFilterState;
typedef struct NICInfo NICInfo;
typedef struct Object Object;
typedef struct ObjectClass ObjectClass;
typedef struct PCIBridge PCIBridge;
typedef struct PCIBus PCIBus;
typedef struct PCIDevice PCIDevice;
typedef struct PCIEPort PCIEPort;
typedef struct PCIESlot PCIESlot;
typedef struct PCIExpressDevice PCIExpressDevice;
typedef struct PCIExpressHost PCIExpressHost;
typedef struct PCIHostDeviceAddress PCIHostDeviceAddress;
typedef struct PCIHostState PCIHostState;
typedef struct Property Property;
typedef struct PropertyInfo PropertyInfo;
typedef struct QBool QBool;
typedef struct QDict QDict;
typedef struct QEMUBH QEMUBH;
typedef struct QemuConsole QemuConsole;
typedef struct QEMUCursor QEMUCursor;
typedef struct QEMUFile QEMUFile;
typedef struct QemuMutex QemuMutex;
typedef struct QemuOpts QemuOpts;
typedef struct QemuOptsList QemuOptsList;
typedef struct QEMUSGList QEMUSGList;
typedef struct QemuSpin QemuSpin;
typedef struct QEMUTimer QEMUTimer;
typedef struct QEMUTimerListGroup QEMUTimerListGroup;
typedef struct QList QList;
typedef struct QNull QNull;
typedef struct QNum QNum;
typedef struct QObject QObject;
typedef struct QString QString;
typedef struct RAMBlock RAMBlock;
typedef struct Range Range;
typedef struct ReservedRegion ReservedRegion;
typedef struct SHPCDevice SHPCDevice;
typedef struct SSIBus SSIBus;
typedef struct TCGCPUOps TCGCPUOps;
typedef struct TCGHelperInfo TCGHelperInfo;
typedef struct TaskState TaskState;
typedef struct TranslationBlock TranslationBlock;
typedef struct VirtIODevice VirtIODevice;
typedef struct Visitor Visitor;
typedef struct VMChangeStateEntry VMChangeStateEntry;
typedef struct VMStateDescription VMStateDescription;
typedef struct IRQState *qemu_irq;
typedef void (*qemu_irq_handler)(void *opaque, int n, int level);
__attribute__ ((__noreturn__))
void __attribute__((error("code path is reachable")))
    qemu_build_not_reached_always(void);
int qemu_daemon(int nochdir, int noclose);
void *qemu_anon_ram_alloc(size_t size, uint64_t *align, 
                                                       _Bool 
                                                            shared,
                          
                         _Bool 
                              noreserve);
void qemu_anon_ram_free(void *ptr, size_t size);
struct qemu_signalfd_siginfo {
    uint32_t ssi_signo;
    int32_t ssi_errno;
    int32_t ssi_code;
    uint32_t ssi_pid;
    uint32_t ssi_uid;
    int32_t ssi_fd;
    uint32_t ssi_tid;
    uint32_t ssi_band;
    uint32_t ssi_overrun;
    uint32_t ssi_trapno;
    int32_t ssi_status;
    int32_t ssi_int;
    uint64_t ssi_ptr;
    uint64_t ssi_utime;
    uint64_t ssi_stime;
    uint64_t ssi_addr;
    uint8_t pad[48];
};
int qemu_signalfd(const sigset_t *mask);
void sigaction_invoke(struct sigaction *action,
                      struct qemu_signalfd_siginfo *info);
int qemu_open_old(const char *name, int flags, ...);
int qemu_open(const char *name, int flags, Error **errp);
int qemu_create(const char *name, int flags, mode_t mode, Error **errp);
int qemu_close(int fd);
int qemu_unlink(const char *name);
int qemu_dup_flags(int fd, int flags);
int qemu_dup(int fd);
int qemu_lock_fd(int fd, int64_t start, int64_t len, 
                                                    _Bool 
                                                         exclusive);
int qemu_unlock_fd(int fd, int64_t start, int64_t len);
int qemu_lock_fd_test(int fd, int64_t start, int64_t len, 
                                                         _Bool 
                                                              exclusive);
_Bool 
    qemu_has_ofd_lock(void);
_Bool 
    qemu_has_direct_io(void);
_Bool 
    qemu_write_pidfile(const char *pidfile, Error **errp);
int qemu_get_thread_id(void);
ssize_t qemu_write_full(int fd, const void *buf, size_t count)
    __attribute__((warn_unused_result));
void qemu_set_cloexec(int fd);
char *qemu_get_local_state_dir(void);
unsigned long qemu_getauxval(unsigned long type);
void qemu_set_tty_echo(int fd, 
                              _Bool 
                                   echo);
typedef struct ThreadContext ThreadContext;
_Bool 
    qemu_prealloc_mem(int fd, char *area, size_t sz, int max_threads,
                       ThreadContext *tc, 
                                         _Bool 
                                              async, Error **errp);
_Bool 
    qemu_finish_async_prealloc_mem(Error **errp);
char *qemu_get_pid_name(pid_t pid);
int qemu_fdatasync(int fd);
void qemu_close_all_open_fd(const int *skip, unsigned int nskip);
int qemu_msync(void *addr, size_t length, int fd);
size_t qemu_get_host_physmem(void);
typedef uint64_t hwaddr;
typedef struct MemMapEntry {
    hwaddr base;
    hwaddr size;
} MemMapEntry;
typedef uint64_t vaddr;
typedef int64_t aligned_int64_t __attribute__((aligned(8)));
typedef uint64_t aligned_uint64_t __attribute__((aligned(8)));
typedef struct QTailQLink {
    void *tql_next;
    struct QTailQLink *tql_prev;
} QTailQLink;
unsigned long find_last_bit(const unsigned long *addr,
                            unsigned long size);
unsigned long find_next_bit(const unsigned long *addr,
                            unsigned long size,
                            unsigned long offset);
unsigned long find_next_zero_bit(const unsigned long *addr,
                                 unsigned long size,
                                 unsigned long offset);
int slow_bitmap_empty(const unsigned long *bitmap, long bits);
int slow_bitmap_full(const unsigned long *bitmap, long bits);
int slow_bitmap_equal(const unsigned long *bitmap1,
                      const unsigned long *bitmap2, long bits);
void slow_bitmap_complement(unsigned long *dst, const unsigned long *src,
                            long bits);
int slow_bitmap_and(unsigned long *dst, const unsigned long *bitmap1,
                    const unsigned long *bitmap2, long bits);
void slow_bitmap_or(unsigned long *dst, const unsigned long *bitmap1,
                    const unsigned long *bitmap2, long bits);
void slow_bitmap_xor(unsigned long *dst, const unsigned long *bitmap1,
                     const unsigned long *bitmap2, long bits);
int slow_bitmap_andnot(unsigned long *dst, const unsigned long *bitmap1,
                       const unsigned long *bitmap2, long bits);
int slow_bitmap_intersects(const unsigned long *bitmap1,
                           const unsigned long *bitmap2, long bits);
long slow_bitmap_count_one(const unsigned long *bitmap, long nbits);
void bitmap_set(unsigned long *map, long i, long len);
void bitmap_set_atomic(unsigned long *map, long i, long len);
void bitmap_clear(unsigned long *map, long start, long nr);
_Bool 
    bitmap_test_and_clear_atomic(unsigned long *map, long start, long nr);
_Bool 
    bitmap_test_and_clear(unsigned long *map, long start, long nr);
void bitmap_copy_and_clear_atomic(unsigned long *dst, unsigned long *src,
                                  long nr);
unsigned long bitmap_find_next_zero_area(unsigned long *map,
                                         unsigned long size,
                                         unsigned long start,
                                         unsigned long nr,
                                         unsigned long align_mask);
void bitmap_to_le(unsigned long *dst, const unsigned long *src,
                  long nbits);
void bitmap_from_le(unsigned long *dst, const unsigned long *src,
                    long nbits);
void bitmap_copy_with_src_offset(unsigned long *dst, const unsigned long *src,
                                 unsigned long offset, unsigned long nbits);
void bitmap_copy_with_dst_offset(unsigned long *dst, const unsigned long *src,
                                 unsigned long shift, unsigned long nbits);
typedef struct QemuCond QemuCond;
typedef struct QemuSemaphore QemuSemaphore;
typedef struct QemuEvent QemuEvent;
typedef struct QemuLockCnt QemuLockCnt;
typedef struct QemuThread QemuThread;
struct QemuMutex {
    pthread_mutex_t lock;
    
   _Bool 
        initialized;
};
typedef struct QemuRecMutex {
    QemuMutex m;
} QemuRecMutex;
struct QemuCond {
    pthread_cond_t cond;
    
   _Bool 
        initialized;
};
struct QemuSemaphore {
    QemuMutex mutex;
    QemuCond cond;
    unsigned int count;
};
struct QemuEvent {
    unsigned value;
    
   _Bool 
        initialized;
};
struct QemuThread {
    pthread_t thread;
};
enum QSPSortBy {
    QSP_SORT_BY_TOTAL_WAIT_TIME,
    QSP_SORT_BY_AVG_WAIT_TIME,
};
void qsp_report(size_t max, enum QSPSortBy sort_by,
                
               _Bool 
                    callsite_coalesce);
_Bool 
    qsp_is_enabled(void);
void qsp_enable(void);
void qsp_disable(void);
void qsp_reset(void);
void qemu_mutex_init(QemuMutex *mutex);
void qemu_mutex_destroy(QemuMutex *mutex);
int qemu_mutex_trylock_impl(QemuMutex *mutex, const char *file,
                                       const int line);
void qemu_mutex_lock_impl(QemuMutex *mutex, const char *file,
                                     const int line);
void qemu_mutex_unlock_impl(QemuMutex *mutex, const char *file,
                                       const int line);
void qemu_rec_mutex_init(QemuRecMutex *mutex);
void qemu_rec_mutex_destroy(QemuRecMutex *mutex);
void qemu_rec_mutex_lock_impl(QemuRecMutex *mutex, const char *file, int line);
int qemu_rec_mutex_trylock_impl(QemuRecMutex *mutex, const char *file, int line);
void qemu_rec_mutex_unlock_impl(QemuRecMutex *mutex, const char *file, int line);
typedef void (*QemuMutexLockFunc)(QemuMutex *m, const char *f, int l);
typedef int (*QemuMutexTrylockFunc)(QemuMutex *m, const char *f, int l);
typedef void (*QemuRecMutexLockFunc)(QemuRecMutex *m, const char *f, int l);
typedef int (*QemuRecMutexTrylockFunc)(QemuRecMutex *m, const char *f, int l);
typedef void (*QemuCondWaitFunc)(QemuCond *c, QemuMutex *m, const char *f,
                                 int l);
typedef 
       _Bool 
            (*QemuCondTimedWaitFunc)(QemuCond *c, QemuMutex *m, int ms,
                                      const char *f, int l);
extern QemuMutexLockFunc bql_mutex_lock_func;
extern QemuMutexLockFunc qemu_mutex_lock_func;
extern QemuMutexTrylockFunc qemu_mutex_trylock_func;
extern QemuRecMutexLockFunc qemu_rec_mutex_lock_func;
extern QemuRecMutexTrylockFunc qemu_rec_mutex_trylock_func;
extern QemuCondWaitFunc qemu_cond_wait_func;
extern QemuCondTimedWaitFunc qemu_cond_timedwait_func;
void qemu_cond_init(QemuCond *cond);
void qemu_cond_destroy(QemuCond *cond);
void qemu_cond_signal(QemuCond *cond);
void qemu_cond_broadcast(QemuCond *cond);
void qemu_cond_wait_impl(QemuCond *cond, QemuMutex *mutex,
                                    const char *file, const int line);
_Bool 
    qemu_cond_timedwait_impl(QemuCond *cond, QemuMutex *mutex, int ms,
                              const char *file, const int line);
void qemu_sem_init(QemuSemaphore *sem, int init);
void qemu_sem_post(QemuSemaphore *sem);
void qemu_sem_wait(QemuSemaphore *sem);
int qemu_sem_timedwait(QemuSemaphore *sem, int ms);
void qemu_sem_destroy(QemuSemaphore *sem);
void qemu_event_init(QemuEvent *ev, 
                                   _Bool 
                                        init);
void qemu_event_set(QemuEvent *ev);
void qemu_event_reset(QemuEvent *ev);
void qemu_event_wait(QemuEvent *ev);
void qemu_event_destroy(QemuEvent *ev);
void qemu_thread_create(QemuThread *thread, const char *name,
                        void *(*start_routine)(void *),
                        void *arg, int mode);
int qemu_thread_set_affinity(QemuThread *thread, unsigned long *host_cpus,
                             unsigned long nbits);
int qemu_thread_get_affinity(QemuThread *thread, unsigned long **host_cpus,
                             unsigned long *nbits);
void *qemu_thread_join(QemuThread *thread);
void qemu_thread_get_self(QemuThread *thread);
_Bool 
    qemu_thread_is_self(QemuThread *thread);
__attribute__ ((__noreturn__)) void qemu_thread_exit(void *retval);
void qemu_thread_naming(
                       _Bool 
                            enable);
struct Notifier;
void qemu_thread_atexit_add(struct Notifier *notifier);
void qemu_thread_atexit_remove(struct Notifier *notifier);
struct QemuSpin {
    int value;
};
typedef struct Notifier Notifier;
struct Notifier
{
    void (*notify)(Notifier *notifier, void *data);
    struct { struct Notifier *le_next; struct Notifier **le_prev; } node;
};
typedef struct NotifierList
{
    struct { struct Notifier *lh_first; } notifiers;
} NotifierList;
void notifier_list_init(NotifierList *list);
void notifier_list_add(NotifierList *list, Notifier *notifier);
void notifier_remove(Notifier *notifier);
void notifier_list_notify(NotifierList *list, void *data);
_Bool 
    notifier_list_empty(NotifierList *list);
typedef struct NotifierWithReturn NotifierWithReturn;
typedef int (*NotifierWithReturnFunc)(NotifierWithReturn *notifier, void *data,
                                      Error **errp);
struct NotifierWithReturn {
    NotifierWithReturnFunc notify;
    struct { struct NotifierWithReturn *le_next; struct NotifierWithReturn **le_prev; } node;
};
typedef struct NotifierWithReturnList {
    struct { struct NotifierWithReturn *lh_first; } notifiers;
} NotifierWithReturnList;
void notifier_with_return_list_init(NotifierWithReturnList *list);
void notifier_with_return_list_add(NotifierWithReturnList *list,
                                   NotifierWithReturn *notifier);
void notifier_with_return_remove(NotifierWithReturn *notifier);
int notifier_with_return_list_notify(NotifierWithReturnList *list,
                                     void *data, Error **errp);
extern unsigned long rcu_gp_ctr;
extern QemuEvent rcu_gp_event;
struct rcu_reader_data {
    unsigned long ctr;
    
   _Bool 
        waiting;
    unsigned depth;
    struct { struct rcu_reader_data *le_next; struct rcu_reader_data **le_prev; } node;
    NotifierList force_rcu;
};
__attribute__((noinline)) struct rcu_reader_data get_rcu_reader(void); __attribute__((noinline)) void set_rcu_reader(struct rcu_reader_data v); __attribute__((noinline)) struct rcu_reader_data *get_ptr_rcu_reader(void);
void synchronize_rcu(void);
void rcu_register_thread(void);
void rcu_unregister_thread(void);
void rcu_enable_atfork(void);
void rcu_disable_atfork(void);
struct rcu_head;
typedef void RCUCBFunc(struct rcu_head *head);
struct rcu_head {
    struct rcu_head *next;
    RCUCBFunc *func;
};
void call_rcu1(struct rcu_head *head, RCUCBFunc *func);
void drain_call_rcu(void);
typedef void RCUReadAuto;
typedef RCUReadAuto *RCUReadAuto_autoptr; typedef GList *RCUReadAuto_listautoptr; typedef GSList *RCUReadAuto_slistautoptr; typedef GQueue *RCUReadAuto_queueautoptr;
void rcu_add_force_rcu_notifier(Notifier *n);
void rcu_remove_force_rcu_notifier(Notifier *n);
typedef enum {
    QAPI_DEPRECATED,
    QAPI_UNSTABLE,
} QapiSpecialFeature;
typedef struct QEnumLookup {
    const char *const *array;
    const unsigned char *const special_features;
    const int size;
} QEnumLookup;
const char *qapi_enum_lookup(const QEnumLookup *lookup, int val);
int qapi_enum_parse(const QEnumLookup *lookup, const char *buf,
                    int def, Error **errp);
_Bool 
    qapi_bool_parse(const char *name, const char *value, 
                                                         _Bool 
                                                              *obj,
                     Error **errp);
int parse_qapi_name(const char *name, 
                                     _Bool 
                                          complete);
typedef struct strList strList;
typedef struct numberList numberList;
typedef struct intList intList;
typedef struct int8List int8List;
typedef struct int16List int16List;
typedef struct int32List int32List;
typedef struct int64List int64List;
typedef struct uint8List uint8List;
typedef struct uint16List uint16List;
typedef struct uint32List uint32List;
typedef struct uint64List uint64List;
typedef struct sizeList sizeList;
typedef struct boolList boolList;
typedef struct anyList anyList;
typedef struct nullList nullList;
typedef enum QType {
    QTYPE_NONE,
    QTYPE_QNULL,
    QTYPE_QNUM,
    QTYPE_QSTRING,
    QTYPE_QDICT,
    QTYPE_QLIST,
    QTYPE_QBOOL,
    QTYPE__MAX,
} QType;
extern const QEnumLookup QType_lookup;
struct strList {
    strList *next;
    char *value;
};
void qapi_free_strList(strList *obj);
typedef strList *strList_autoptr; typedef GList *strList_listautoptr; typedef GSList *strList_slistautoptr; typedef GQueue *strList_queueautoptr;
struct numberList {
    numberList *next;
    double value;
};
void qapi_free_numberList(numberList *obj);
typedef numberList *numberList_autoptr; typedef GList *numberList_listautoptr; typedef GSList *numberList_slistautoptr; typedef GQueue *numberList_queueautoptr;
struct intList {
    intList *next;
    int64_t value;
};
void qapi_free_intList(intList *obj);
typedef intList *intList_autoptr; typedef GList *intList_listautoptr; typedef GSList *intList_slistautoptr; typedef GQueue *intList_queueautoptr;
struct int8List {
    int8List *next;
    int8_t value;
};
void qapi_free_int8List(int8List *obj);
typedef int8List *int8List_autoptr; typedef GList *int8List_listautoptr; typedef GSList *int8List_slistautoptr; typedef GQueue *int8List_queueautoptr;
struct int16List {
    int16List *next;
    int16_t value;
};
void qapi_free_int16List(int16List *obj);
typedef int16List *int16List_autoptr; typedef GList *int16List_listautoptr; typedef GSList *int16List_slistautoptr; typedef GQueue *int16List_queueautoptr;
struct int32List {
    int32List *next;
    int32_t value;
};
void qapi_free_int32List(int32List *obj);
typedef int32List *int32List_autoptr; typedef GList *int32List_listautoptr; typedef GSList *int32List_slistautoptr; typedef GQueue *int32List_queueautoptr;
struct int64List {
    int64List *next;
    int64_t value;
};
void qapi_free_int64List(int64List *obj);
typedef int64List *int64List_autoptr; typedef GList *int64List_listautoptr; typedef GSList *int64List_slistautoptr; typedef GQueue *int64List_queueautoptr;
struct uint8List {
    uint8List *next;
    uint8_t value;
};
void qapi_free_uint8List(uint8List *obj);
typedef uint8List *uint8List_autoptr; typedef GList *uint8List_listautoptr; typedef GSList *uint8List_slistautoptr; typedef GQueue *uint8List_queueautoptr;
struct uint16List {
    uint16List *next;
    uint16_t value;
};
void qapi_free_uint16List(uint16List *obj);
typedef uint16List *uint16List_autoptr; typedef GList *uint16List_listautoptr; typedef GSList *uint16List_slistautoptr; typedef GQueue *uint16List_queueautoptr;
struct uint32List {
    uint32List *next;
    uint32_t value;
};
void qapi_free_uint32List(uint32List *obj);
typedef uint32List *uint32List_autoptr; typedef GList *uint32List_listautoptr; typedef GSList *uint32List_slistautoptr; typedef GQueue *uint32List_queueautoptr;
struct uint64List {
    uint64List *next;
    uint64_t value;
};
void qapi_free_uint64List(uint64List *obj);
typedef uint64List *uint64List_autoptr; typedef GList *uint64List_listautoptr; typedef GSList *uint64List_slistautoptr; typedef GQueue *uint64List_queueautoptr;
struct sizeList {
    sizeList *next;
    uint64_t value;
};
void qapi_free_sizeList(sizeList *obj);
typedef sizeList *sizeList_autoptr; typedef GList *sizeList_listautoptr; typedef GSList *sizeList_slistautoptr; typedef GQueue *sizeList_queueautoptr;
struct boolList {
    boolList *next;
    
   _Bool 
        value;
};
void qapi_free_boolList(boolList *obj);
typedef boolList *boolList_autoptr; typedef GList *boolList_listautoptr; typedef GSList *boolList_slistautoptr; typedef GQueue *boolList_queueautoptr;
struct anyList {
    anyList *next;
    QObject *value;
};
void qapi_free_anyList(anyList *obj);
typedef anyList *anyList_autoptr; typedef GList *anyList_listautoptr; typedef GSList *anyList_slistautoptr; typedef GQueue *anyList_queueautoptr;
struct nullList {
    nullList *next;
    QNull *value;
};
void qapi_free_nullList(nullList *obj);
typedef nullList *nullList_autoptr; typedef GList *nullList_listautoptr; typedef GSList *nullList_slistautoptr; typedef GQueue *nullList_queueautoptr;
typedef enum {
    MODULE_INIT_MIGRATION,
    MODULE_INIT_BLOCK,
    MODULE_INIT_OPTS,
    MODULE_INIT_QOM,
    MODULE_INIT_TRACE,
    MODULE_INIT_XEN_BACKEND,
    MODULE_INIT_LIBQOS,
    MODULE_INIT_FUZZ_TARGET,
    MODULE_INIT_MAX
} module_init_type;
void register_module_init(void (*fn)(void), module_init_type type);
void register_dso_module_init(void (*fn)(void), module_init_type type);
void module_call_init(module_init_type type);
int module_load(const char *prefix, const char *name, Error **errp);
int module_load_qom(const char *type, Error **errp);
void module_load_qom_all(void);
void module_allow_arch(const char *arch);
typedef struct QemuModinfo QemuModinfo;
struct QemuModinfo {
    const char *name;
    const char *arch;
    const char **objs;
    const char **deps;
    const char **opts;
};
extern const QemuModinfo qemu_modinfo[];
void module_init_info(const QemuModinfo *info);
struct TypeImpl;
typedef struct TypeImpl *Type;
typedef struct TypeInfo TypeInfo;
typedef struct InterfaceClass InterfaceClass;
typedef struct InterfaceInfo InterfaceInfo;
typedef struct ObjectProperty ObjectProperty;
typedef void (ObjectPropertyAccessor)(Object *obj,
                                      Visitor *v,
                                      const char *name,
                                      void *opaque,
                                      Error **errp);
typedef Object *(ObjectPropertyResolve)(Object *obj,
                                        void *opaque,
                                        const char *part);
typedef void (ObjectPropertyRelease)(Object *obj,
                                     const char *name,
                                     void *opaque);
typedef void (ObjectPropertyInit)(Object *obj, ObjectProperty *prop);
struct ObjectProperty
{
    char *name;
    char *type;
    char *description;
    ObjectPropertyAccessor *get;
    ObjectPropertyAccessor *set;
    ObjectPropertyResolve *resolve;
    ObjectPropertyRelease *release;
    ObjectPropertyInit *init;
    void *opaque;
    QObject *defval;
};
typedef void (ObjectUnparent)(Object *obj);
typedef void (ObjectFree)(void *obj);
struct ObjectClass
{
    Type type;
    GSList *interfaces;
    const char *object_cast_cache[4];
    const char *class_cast_cache[4];
    ObjectUnparent *unparent;
    GHashTable *properties;
};
struct Object
{
    ObjectClass *class;
    ObjectFree *free;
    GHashTable *properties;
    uint32_t ref;
    Object *parent;
};
struct TypeInfo
{
    const char *name;
    const char *parent;
    size_t instance_size;
    size_t instance_align;
    void (*instance_init)(Object *obj);
    void (*instance_post_init)(Object *obj);
    void (*instance_finalize)(Object *obj);
    
   _Bool 
        abstract;
    size_t class_size;
    void (*class_init)(ObjectClass *klass, void *data);
    void (*class_base_init)(ObjectClass *klass, void *data);
    void *class_data;
    InterfaceInfo *interfaces;
};
struct InterfaceInfo {
    const char *type;
};
struct InterfaceClass
{
    ObjectClass parent_class;
    Type interface_type;
};
Object *object_new_with_class(ObjectClass *klass);
Object *object_new(const char *typename);
Object *object_new_with_props(const char *typename,
                              Object *parent,
                              const char *id,
                              Error **errp,
                              ...) __attribute__((__sentinel__));
Object *object_new_with_propv(const char *typename,
                              Object *parent,
                              const char *id,
                              Error **errp,
                              va_list vargs);
_Bool 
    object_apply_global_props(Object *obj, const GPtrArray *props,
                               Error **errp);
void object_set_machine_compat_props(GPtrArray *compat_props);
void object_set_accelerator_compat_props(GPtrArray *compat_props);
void object_register_sugar_prop(const char *driver, const char *prop,
                                const char *value, 
                                                  _Bool 
                                                       optional);
void object_apply_compat_props(Object *obj);
_Bool 
    object_set_props(Object *obj, Error **errp, ...) __attribute__((__sentinel__));
_Bool 
    object_set_propv(Object *obj, Error **errp, va_list vargs);
void object_initialize(void *obj, size_t size, const char *typename);
_Bool 
    object_initialize_child_with_props(Object *parentobj,
                             const char *propname,
                             void *childobj, size_t size, const char *type,
                             Error **errp, ...) __attribute__((__sentinel__));
_Bool 
    object_initialize_child_with_propsv(Object *parentobj,
                              const char *propname,
                              void *childobj, size_t size, const char *type,
                              Error **errp, va_list vargs);
void object_initialize_child_internal(Object *parent, const char *propname,
                                      void *child, size_t size,
                                      const char *type);
Object *object_dynamic_cast(Object *obj, const char *typename);
Object *object_dynamic_cast_assert(Object *obj, const char *typename,
                                   const char *file, int line, const char *func);
ObjectClass *object_get_class(Object *obj);
const char *object_get_typename(const Object *obj);
Type type_register_static(const TypeInfo *info);
void type_register_static_array(const TypeInfo *infos, int nr_infos);
_Bool 
    type_print_class_properties(const char *type);
void object_set_properties_from_keyval(Object *obj, const QDict *qdict,
                                       
                                      _Bool 
                                           from_json, Error **errp);
ObjectClass *object_class_dynamic_cast_assert(ObjectClass *klass,
                                              const char *typename,
                                              const char *file, int line,
                                              const char *func);
ObjectClass *object_class_dynamic_cast(ObjectClass *klass,
                                       const char *typename);
ObjectClass *object_class_get_parent(ObjectClass *klass);
const char *object_class_get_name(ObjectClass *klass);
_Bool 
    object_class_is_abstract(ObjectClass *klass);
ObjectClass *object_class_by_name(const char *typename);
ObjectClass *module_object_class_by_name(const char *typename);
void object_class_foreach(void (*fn)(ObjectClass *klass, void *opaque),
                          const char *implements_type, 
                                                      _Bool 
                                                           include_abstract,
                          void *opaque);
GSList *object_class_get_list(const char *implements_type,
                              
                             _Bool 
                                  include_abstract);
GSList *object_class_get_list_sorted(const char *implements_type,
                              
                             _Bool 
                                  include_abstract);
Object *object_ref(void *obj);
void object_unref(void *obj);
ObjectProperty *object_property_try_add(Object *obj, const char *name,
                                        const char *type,
                                        ObjectPropertyAccessor *get,
                                        ObjectPropertyAccessor *set,
                                        ObjectPropertyRelease *release,
                                        void *opaque, Error **errp);
ObjectProperty *object_property_add(Object *obj, const char *name,
                                    const char *type,
                                    ObjectPropertyAccessor *get,
                                    ObjectPropertyAccessor *set,
                                    ObjectPropertyRelease *release,
                                    void *opaque);
void object_property_del(Object *obj, const char *name);
ObjectProperty *object_class_property_add(ObjectClass *klass, const char *name,
                                          const char *type,
                                          ObjectPropertyAccessor *get,
                                          ObjectPropertyAccessor *set,
                                          ObjectPropertyRelease *release,
                                          void *opaque);
void object_property_set_default_bool(ObjectProperty *prop, 
                                                           _Bool 
                                                                value);
void object_property_set_default_str(ObjectProperty *prop, const char *value);
void object_property_set_default_list(ObjectProperty *prop);
void object_property_set_default_int(ObjectProperty *prop, int64_t value);
void object_property_set_default_uint(ObjectProperty *prop, uint64_t value);
ObjectProperty *object_property_find(Object *obj, const char *name);
ObjectProperty *object_property_find_err(Object *obj,
                                         const char *name,
                                         Error **errp);
ObjectProperty *object_class_property_find(ObjectClass *klass,
                                           const char *name);
ObjectProperty *object_class_property_find_err(ObjectClass *klass,
                                               const char *name,
                                               Error **errp);
typedef struct ObjectPropertyIterator {
    ObjectClass *nextclass;
    GHashTableIter iter;
} ObjectPropertyIterator;
void object_property_iter_init(ObjectPropertyIterator *iter,
                               Object *obj);
void object_class_property_iter_init(ObjectPropertyIterator *iter,
                                     ObjectClass *klass);
ObjectProperty *object_property_iter_next(ObjectPropertyIterator *iter);
void object_unparent(Object *obj);
_Bool 
    object_property_get(Object *obj, const char *name, Visitor *v,
                         Error **errp);
_Bool 
    object_property_set_str(Object *obj, const char *name,
                             const char *value, Error **errp);
char *object_property_get_str(Object *obj, const char *name,
                              Error **errp);
_Bool 
    object_property_set_link(Object *obj, const char *name,
                              Object *value, Error **errp);
Object *object_property_get_link(Object *obj, const char *name,
                                 Error **errp);
_Bool 
    object_property_set_bool(Object *obj, const char *name,
                              
                             _Bool 
                                  value, Error **errp);
_Bool 
    object_property_get_bool(Object *obj, const char *name,
                              Error **errp);
_Bool 
    object_property_set_int(Object *obj, const char *name,
                             int64_t value, Error **errp);
int64_t object_property_get_int(Object *obj, const char *name,
                                Error **errp);
_Bool 
    object_property_set_uint(Object *obj, const char *name,
                              uint64_t value, Error **errp);
uint64_t object_property_get_uint(Object *obj, const char *name,
                                  Error **errp);
int object_property_get_enum(Object *obj, const char *name,
                             const char *typename, Error **errp);
_Bool 
    object_property_set(Object *obj, const char *name, Visitor *v,
                         Error **errp);
_Bool 
    object_property_parse(Object *obj, const char *name,
                           const char *string, Error **errp);
char *object_property_print(Object *obj, const char *name, 
                                                          _Bool 
                                                               human,
                            Error **errp);
const char *object_property_get_type(Object *obj, const char *name,
                                     Error **errp);
Object *object_get_root(void);
Object *object_get_container(const char *name);
Object *object_get_objects_root(void);
Object *object_get_internal_root(void);
const char *object_get_canonical_path_component(const Object *obj);
char *object_get_canonical_path(const Object *obj);
Object *object_resolve_path(const char *path, 
                                             _Bool 
                                                  *ambiguous);
Object *object_resolve_path_type(const char *path, const char *typename,
                                 
                                _Bool 
                                     *ambiguous);
Object *object_resolve_type_unambiguous(const char *typename, Error **errp);
Object *object_resolve_path_at(Object *parent, const char *path);
Object *object_resolve_path_component(Object *parent, const char *part);
ObjectProperty *object_property_try_add_child(Object *obj, const char *name,
                                              Object *child, Error **errp);
ObjectProperty *object_property_add_child(Object *obj, const char *name,
                                          Object *child);
typedef enum {
    OBJ_PROP_LINK_STRONG = 0x1,
    OBJ_PROP_LINK_DIRECT = 0x2,
    OBJ_PROP_LINK_CLASS = 0x4,
} ObjectPropertyLinkFlags;
void object_property_allow_set_link(const Object *obj, const char *name,
                                    Object *child, Error **errp);
ObjectProperty *object_property_add_link(Object *obj, const char *name,
                              const char *type, Object **targetp,
                              void (*check)(const Object *obj, const char *name,
                                            Object *val, Error **errp),
                              ObjectPropertyLinkFlags flags);
ObjectProperty *object_class_property_add_link(ObjectClass *oc,
                              const char *name,
                              const char *type, ptrdiff_t offset,
                              void (*check)(const Object *obj, const char *name,
                                            Object *val, Error **errp),
                              ObjectPropertyLinkFlags flags);
ObjectProperty *object_property_add_str(Object *obj, const char *name,
                             char *(*get)(Object *, Error **),
                             void (*set)(Object *, const char *, Error **));
ObjectProperty *object_class_property_add_str(ObjectClass *klass,
                                   const char *name,
                                   char *(*get)(Object *, Error **),
                                   void (*set)(Object *, const char *,
                                               Error **));
ObjectProperty *object_property_add_bool(Object *obj, const char *name,
                              
                             _Bool 
                                  (*get)(Object *, Error **),
                              void (*set)(Object *, 
                                                   _Bool
                                                       , Error **));
ObjectProperty *object_class_property_add_bool(ObjectClass *klass,
                                    const char *name,
                                    
                                   _Bool 
                                        (*get)(Object *, Error **),
                                    void (*set)(Object *, 
                                                         _Bool
                                                             , Error **));
ObjectProperty *object_property_add_enum(Object *obj, const char *name,
                              const char *typename,
                              const QEnumLookup *lookup,
                              int (*get)(Object *, Error **),
                              void (*set)(Object *, int, Error **));
ObjectProperty *object_class_property_add_enum(ObjectClass *klass,
                                    const char *name,
                                    const char *typename,
                                    const QEnumLookup *lookup,
                                    int (*get)(Object *, Error **),
                                    void (*set)(Object *, int, Error **));
ObjectProperty *object_property_add_tm(Object *obj, const char *name,
                            void (*get)(Object *, struct tm *, Error **));
ObjectProperty *object_class_property_add_tm(ObjectClass *klass,
                            const char *name,
                            void (*get)(Object *, struct tm *, Error **));
typedef enum {
    OBJ_PROP_FLAG_READ = 1 << 0,
    OBJ_PROP_FLAG_WRITE = 1 << 1,
    OBJ_PROP_FLAG_READWRITE = (OBJ_PROP_FLAG_READ | OBJ_PROP_FLAG_WRITE),
} ObjectPropertyFlags;
ObjectProperty *object_property_add_uint8_ptr(Object *obj, const char *name,
                                              const uint8_t *v,
                                              ObjectPropertyFlags flags);
ObjectProperty *object_class_property_add_uint8_ptr(ObjectClass *klass,
                                         const char *name,
                                         const uint8_t *v,
                                         ObjectPropertyFlags flags);
ObjectProperty *object_property_add_uint16_ptr(Object *obj, const char *name,
                                    const uint16_t *v,
                                    ObjectPropertyFlags flags);
ObjectProperty *object_class_property_add_uint16_ptr(ObjectClass *klass,
                                          const char *name,
                                          const uint16_t *v,
                                          ObjectPropertyFlags flags);
ObjectProperty *object_property_add_uint32_ptr(Object *obj, const char *name,
                                    const uint32_t *v,
                                    ObjectPropertyFlags flags);
ObjectProperty *object_class_property_add_uint32_ptr(ObjectClass *klass,
                                          const char *name,
                                          const uint32_t *v,
                                          ObjectPropertyFlags flags);
ObjectProperty *object_property_add_uint64_ptr(Object *obj, const char *name,
                                    const uint64_t *v,
                                    ObjectPropertyFlags flags);
ObjectProperty *object_class_property_add_uint64_ptr(ObjectClass *klass,
                                          const char *name,
                                          const uint64_t *v,
                                          ObjectPropertyFlags flags);
ObjectProperty *object_property_add_alias(Object *obj, const char *name,
                               Object *target_obj, const char *target_name);
ObjectProperty *object_property_add_const_link(Object *obj, const char *name,
                                               Object *target);
void object_property_set_description(Object *obj, const char *name,
                                     const char *description);
void object_class_property_set_description(ObjectClass *klass, const char *name,
                                           const char *description);
int object_child_foreach(Object *obj, int (*fn)(Object *child, void *opaque),
                         void *opaque);
int object_child_foreach_recursive(Object *obj,
                                   int (*fn)(Object *child, void *opaque),
                                   void *opaque);
Object *object_property_add_new_container(Object *obj, const char *name);
char *object_property_help(const char *name, const char *type,
                           QObject *defval, const char *description);
typedef Object *Object_autoptr; typedef GList *Object_listautoptr; typedef GSList *Object_slistautoptr; typedef GQueue *Object_queueautoptr;
typedef struct HotplugHandlerClass HotplugHandlerClass;
typedef struct HotplugHandler HotplugHandler;
typedef void (*hotplug_fn)(HotplugHandler *plug_handler,
                           DeviceState *plugged_dev, Error **errp);
struct HotplugHandlerClass {
    InterfaceClass parent;
    hotplug_fn pre_plug;
    hotplug_fn plug;
    hotplug_fn unplug_request;
    hotplug_fn unplug;
    
   _Bool 
        (*is_hotpluggable_bus)(HotplugHandler *plug_handler, BusState *bus);
};
void hotplug_handler_plug(HotplugHandler *plug_handler,
                          DeviceState *plugged_dev,
                          Error **errp);
void hotplug_handler_pre_plug(HotplugHandler *plug_handler,
                              DeviceState *plugged_dev,
                              Error **errp);
void hotplug_handler_unplug_request(HotplugHandler *plug_handler,
                                    DeviceState *plugged_dev,
                                    Error **errp);
void hotplug_handler_unplug(HotplugHandler *plug_handler,
                            DeviceState *plugged_dev,
                            Error **errp);
typedef struct ResettableClass ResettableClass;
typedef struct ResettableState ResettableState;
typedef enum ResetType {
    RESET_TYPE_COLD,
    RESET_TYPE_SNAPSHOT_LOAD,
    RESET_TYPE_WAKEUP,
    RESET_TYPE_S390_CPU_INITIAL,
    RESET_TYPE_S390_CPU_NORMAL,
} ResetType;
typedef void (*ResettableEnterPhase)(Object *obj, ResetType type);
typedef void (*ResettableHoldPhase)(Object *obj, ResetType type);
typedef void (*ResettableExitPhase)(Object *obj, ResetType type);
typedef ResettableState * (*ResettableGetState)(Object *obj);
typedef void (*ResettableChildCallback)(Object *, void *opaque,
                                        ResetType type);
typedef void (*ResettableChildForeach)(Object *obj,
                                       ResettableChildCallback cb,
                                       void *opaque, ResetType type);
typedef struct ResettablePhases {
    ResettableEnterPhase enter;
    ResettableHoldPhase hold;
    ResettableExitPhase exit;
} ResettablePhases;
struct ResettableClass {
    InterfaceClass parent_class;
    ResettablePhases phases;
    ResettableGetState get_state;
    ResettableChildForeach child_foreach;
};
struct ResettableState {
    unsigned count;
    
   _Bool 
        hold_phase_pending;
    
   _Bool 
        exit_phase_in_progress;
};
void resettable_reset(Object *obj, ResetType type);
void resettable_assert_reset(Object *obj, ResetType type);
void resettable_release_reset(Object *obj, ResetType type);
_Bool 
    resettable_is_in_reset(Object *obj);
void resettable_change_parent(Object *obj, Object *newp, Object *oldp);
void resettable_cold_reset_fn(void *opaque);
void resettable_class_set_parent_phases(ResettableClass *rc,
                                        ResettableEnterPhase enter,
                                        ResettableHoldPhase hold,
                                        ResettableExitPhase exit,
                                        ResettablePhases *parent_phases);
enum {
    DEV_NVECTORS_UNSPECIFIED = -1,
};
typedef struct DeviceState DeviceState; typedef struct DeviceClass DeviceClass; typedef DeviceState *DeviceState_autoptr; typedef GList *DeviceState_listautoptr; typedef GSList *DeviceState_slistautoptr; typedef GQueue *DeviceState_queueautoptr;
typedef enum DeviceCategory {
    DEVICE_CATEGORY_BRIDGE,
    DEVICE_CATEGORY_USB,
    DEVICE_CATEGORY_STORAGE,
    DEVICE_CATEGORY_NETWORK,
    DEVICE_CATEGORY_INPUT,
    DEVICE_CATEGORY_DISPLAY,
    DEVICE_CATEGORY_SOUND,
    DEVICE_CATEGORY_MISC,
    DEVICE_CATEGORY_CPU,
    DEVICE_CATEGORY_WATCHDOG,
    DEVICE_CATEGORY_MAX
} DeviceCategory;
typedef void (*DeviceRealize)(DeviceState *dev, Error **errp);
typedef void (*DeviceUnrealize)(DeviceState *dev);
typedef void (*DeviceReset)(DeviceState *dev);
typedef void (*BusRealize)(BusState *bus, Error **errp);
typedef void (*BusUnrealize)(BusState *bus);
typedef int (*DeviceSyncConfig)(DeviceState *dev, Error **errp);
struct DeviceClass {
    ObjectClass parent_class;
    unsigned long categories[(((DEVICE_CATEGORY_MAX) + (8 * sizeof(long)) - 1) / (8 * sizeof(long)))];
    const char *fw_name;
    const char *desc;
    const Property *props_;
    uint16_t props_count_;
    
   _Bool 
        user_creatable;
    
   _Bool 
        hotpluggable;
    DeviceReset legacy_reset;
    DeviceRealize realize;
    DeviceUnrealize unrealize;
    DeviceSyncConfig sync_config;
    const VMStateDescription *vmsd;
    const char *bus_type;
};
typedef struct NamedGPIOList NamedGPIOList;
struct NamedGPIOList {
    char *name;
    qemu_irq *in;
    int num_in;
    int num_out;
    struct { struct NamedGPIOList *le_next; struct NamedGPIOList **le_prev; } node;
};
typedef struct Clock Clock;
typedef struct NamedClockList NamedClockList;
struct NamedClockList {
    char *name;
    Clock *clock;
    
   _Bool 
        output;
    
   _Bool 
        alias;
    struct { struct NamedClockList *le_next; struct NamedClockList **le_prev; } node;
};
typedef struct {
    
   _Bool 
        engaged_in_io;
} MemReentrancyGuard;
typedef struct { struct NamedGPIOList *lh_first; } NamedGPIOListHead;
typedef struct { struct NamedClockList *lh_first; } NamedClockListHead;
typedef struct { struct BusState *lh_first; } BusStateHead;
struct DeviceState {
    Object parent_obj;
    char *id;
    char *canonical_path;
    
   _Bool 
        realized;
    
   _Bool 
        pending_deleted_event;
    int64_t pending_deleted_expires_ms;
    int hotplugged;
    
   _Bool 
        allow_unplug_during_migration;
    BusState *parent_bus;
    NamedGPIOListHead gpios;
    NamedClockListHead clocks;
    BusStateHead child_bus;
    int num_child_bus;
    int instance_id_alias;
    int alias_required_for_version;
    ResettableState reset;
    GSList *unplug_blockers;
    MemReentrancyGuard mem_reentrancy_guard;
};
typedef struct DeviceListener DeviceListener;
struct DeviceListener {
    void (*realize)(DeviceListener *listener, DeviceState *dev);
    void (*unrealize)(DeviceListener *listener, DeviceState *dev);
    
   _Bool 
        (*hide_device)(DeviceListener *listener, const QDict *device_opts,
                        
                       _Bool 
                            from_json, Error **errp);
    union { struct DeviceListener *tqe_next; QTailQLink tqe_circ; } link;
};
struct BusClass {
    ObjectClass parent_class;
    void (*print_dev)(Monitor *mon, DeviceState *dev, int indent);
    char *(*get_dev_path)(DeviceState *dev);
    char *(*get_fw_dev_path)(DeviceState *dev);
    
   _Bool 
        (*check_address)(BusState *bus, DeviceState *dev, Error **errp);
    BusRealize realize;
    BusUnrealize unrealize;
    int max_dev;
    int automatic_ids;
};
typedef struct BusChild {
    struct rcu_head rcu;
    DeviceState *child;
    int index;
    union { struct BusChild *tqe_next; QTailQLink tqe_circ; } sibling;
} BusChild;
typedef union { struct BusChild *tqh_first; QTailQLink tqh_circ; } BusChildHead;
typedef struct { struct BusState *le_next; struct BusState **le_prev; } BusStateEntry;
struct BusState {
    Object obj;
    DeviceState *parent;
    char *name;
    HotplugHandler *hotplug_handler;
    int max_index;
    
   _Bool 
        realized;
    
   _Bool 
        full;
    int num_children;
    BusChildHead children;
    BusStateEntry sibling;
    ResettableState reset;
};
typedef struct GlobalProperty {
    const char *driver;
    const char *property;
    const char *value;
    
   _Bool 
        used;
    
   _Bool 
        optional;
} GlobalProperty;
DeviceState *qdev_new(const char *name);
DeviceState *qdev_try_new(const char *name);
_Bool 
    qdev_realize(DeviceState *dev, BusState *bus, Error **errp);
_Bool 
    qdev_realize_and_unref(DeviceState *dev, BusState *bus, Error **errp);
void qdev_unrealize(DeviceState *dev);
void qdev_set_legacy_instance_id(DeviceState *dev, int alias_id,
                                 int required_for_version);
HotplugHandler *qdev_get_bus_hotplug_handler(DeviceState *dev);
HotplugHandler *qdev_get_machine_hotplug_handler(DeviceState *dev);
_Bool 
    qdev_hotplug_allowed(DeviceState *dev, BusState *bus, Error **errp);
_Bool 
    qdev_hotunplug_allowed(DeviceState *dev, Error **errp);
HotplugHandler *qdev_get_hotplug_handler(DeviceState *dev);
void qdev_unplug(DeviceState *dev, Error **errp);
int qdev_sync_config(DeviceState *dev, Error **errp);
void qdev_simple_device_unplug_cb(HotplugHandler *hotplug_dev,
                                  DeviceState *dev, Error **errp);
void qdev_machine_creation_done(void);
_Bool 
    qdev_machine_modified(void);
void qdev_add_unplug_blocker(DeviceState *dev, Error *reason);
void qdev_del_unplug_blocker(DeviceState *dev, Error *reason);
_Bool 
    qdev_unplug_blocked(DeviceState *dev, Error **errp);
typedef enum {
    GPIO_POLARITY_ACTIVE_LOW,
    GPIO_POLARITY_ACTIVE_HIGH
} GpioPolarity;
qemu_irq qdev_get_gpio_in(DeviceState *dev, int n);
qemu_irq qdev_get_gpio_in_named(DeviceState *dev, const char *name, int n);
void qdev_connect_gpio_out(DeviceState *dev, int n, qemu_irq pin);
void qdev_connect_gpio_out_named(DeviceState *dev, const char *name, int n,
                                 qemu_irq input_pin);
qemu_irq qdev_get_gpio_out_connector(DeviceState *dev, const char *name, int n);
qemu_irq qdev_intercept_gpio_out(DeviceState *dev, qemu_irq icpt,
                                 const char *name, int n);
BusState *qdev_get_child_bus(DeviceState *dev, const char *name);
void qdev_init_gpio_in(DeviceState *dev, qemu_irq_handler handler, int n);
void qdev_init_gpio_out(DeviceState *dev, qemu_irq *pins, int n);
void qdev_init_gpio_out_named(DeviceState *dev, qemu_irq *pins,
                              const char *name, int n);
void qdev_init_gpio_in_named_with_opaque(DeviceState *dev,
                                         qemu_irq_handler handler,
                                         void *opaque,
                                         const char *name, int n);
void qdev_pass_gpios(DeviceState *dev, DeviceState *container,
                     const char *name);
BusState *qdev_get_parent_bus(const DeviceState *dev);
DeviceState *qdev_find_recursive(BusState *bus, const char *id);
typedef int (qbus_walkerfn)(BusState *bus, void *opaque);
typedef int (qdev_walkerfn)(DeviceState *dev, void *opaque);
void qbus_init(void *bus, size_t size, const char *typename,
               DeviceState *parent, const char *name);
BusState *qbus_new(const char *typename, DeviceState *parent, const char *name);
_Bool 
    qbus_realize(BusState *bus, Error **errp);
void qbus_unrealize(BusState *bus);
int qbus_walk_children(BusState *bus,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque);
int qdev_walk_children(DeviceState *dev,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque);
void device_cold_reset(DeviceState *dev);
void bus_cold_reset(BusState *bus);
_Bool 
    device_is_in_reset(DeviceState *dev);
_Bool 
    bus_is_in_reset(BusState *bus);
BusState *sysbus_get_default(void);
char *qdev_get_fw_dev_path(DeviceState *dev);
char *qdev_get_own_fw_dev_path_from_handler(BusState *bus, DeviceState *dev);
void device_class_set_props_n(DeviceClass *dc, const Property *props, size_t n);
void device_class_set_parent_realize(DeviceClass *dc,
                                     DeviceRealize dev_realize,
                                     DeviceRealize *parent_realize);
void device_class_set_legacy_reset(DeviceClass *dc,
                                   DeviceReset dev_reset);
void device_class_set_parent_unrealize(DeviceClass *dc,
                                       DeviceUnrealize dev_unrealize,
                                       DeviceUnrealize *parent_unrealize);
const VMStateDescription *qdev_get_vmsd(DeviceState *dev);
const char *qdev_fw_name(DeviceState *dev);
void qdev_assert_realized_properly(void);
Object *qdev_get_machine(void);
void qdev_create_fake_machine(void);
Object *machine_get_container(const char *name);
char *qdev_get_human_name(DeviceState *dev);
_Bool 
    qdev_set_parent_bus(DeviceState *dev, BusState *bus, Error **errp);
extern 
      _Bool 
           qdev_hot_removed;
char *qdev_get_dev_path(DeviceState *dev);
void qbus_set_hotplug_handler(BusState *bus, Object *handler);
void qbus_set_bus_hotplug_handler(BusState *bus);
void device_listener_register(DeviceListener *listener);
void device_listener_unregister(DeviceListener *listener);
_Bool 
    qdev_should_hide_device(const QDict *opts, 
                                               _Bool 
                                                    from_json, Error **errp);
typedef enum MachineInitPhase {
    PHASE_NO_MACHINE,
    PHASE_MACHINE_CREATED,
    PHASE_ACCEL_CREATED,
    PHASE_LATE_BACKENDS_CREATED,
    PHASE_MACHINE_INITIALIZED,
    PHASE_MACHINE_READY,
} MachineInitPhase;
_Bool 
    phase_check(MachineInitPhase phase);
void phase_advance(MachineInitPhase phase);
typedef void *PTR;
typedef uint64_t bfd_vma;
typedef int64_t bfd_signed_vma;
typedef uint8_t bfd_byte;
enum bfd_flavour {
  bfd_target_unknown_flavour,
  bfd_target_aout_flavour,
  bfd_target_coff_flavour,
  bfd_target_ecoff_flavour,
  bfd_target_elf_flavour,
  bfd_target_ieee_flavour,
  bfd_target_nlm_flavour,
  bfd_target_oasys_flavour,
  bfd_target_tekhex_flavour,
  bfd_target_srec_flavour,
  bfd_target_ihex_flavour,
  bfd_target_som_flavour,
  bfd_target_os9k_flavour,
  bfd_target_versados_flavour,
  bfd_target_msdos_flavour,
  bfd_target_evax_flavour
};
enum bfd_endian { BFD_ENDIAN_BIG, BFD_ENDIAN_LITTLE, BFD_ENDIAN_UNKNOWN };
enum bfd_architecture
{
  bfd_arch_unknown,
  bfd_arch_obscure,
  bfd_arch_m68k,
  bfd_arch_vax,
  bfd_arch_i960,
  bfd_arch_a29k,
  bfd_arch_sparc,
  bfd_arch_mips,
  bfd_arch_i386,
  bfd_arch_we32k,
  bfd_arch_tahoe,
  bfd_arch_i860,
  bfd_arch_romp,
  bfd_arch_alliant,
  bfd_arch_convex,
  bfd_arch_m88k,
  bfd_arch_pyramid,
  bfd_arch_h8300,
  bfd_arch_powerpc,
  bfd_arch_rs6000,
  bfd_arch_hppa,
  bfd_arch_d10v,
  bfd_arch_z8k,
  bfd_arch_h8500,
  bfd_arch_sh,
  bfd_arch_alpha,
  bfd_arch_arm,
  bfd_arch_ns32k,
  bfd_arch_w65,
  bfd_arch_tic30,
  bfd_arch_v850,
  bfd_arch_arc,
  bfd_arch_m32r,
  bfd_arch_mn10200,
  bfd_arch_mn10300,
  bfd_arch_avr,
  bfd_arch_microblaze,
  bfd_arch_moxie,
  bfd_arch_ia64,
  bfd_arch_rx,
  bfd_arch_loongarch,
  bfd_arch_last
  };
typedef struct symbol_cache_entry
{
    const char *name;
    union
    {
        PTR p;
        bfd_vma i;
    } udata;
} asymbol;
typedef int (*fprintf_function)(FILE *f, const char *fmt, ...)
    __attribute__((__format__ (gnu_printf, 2, 3)));
enum dis_insn_type {
  dis_noninsn,
  dis_nonbranch,
  dis_branch,
  dis_condbranch,
  dis_jsr,
  dis_condjsr,
  dis_dref,
  dis_dref2
};
typedef struct disassemble_info {
  fprintf_function fprintf_func;
  FILE *stream;
  PTR application_data;
  enum bfd_flavour flavour;
  enum bfd_architecture arch;
  unsigned long mach;
  enum bfd_endian endian;
  asymbol **symbols;
  int num_symbols;
  unsigned long flags;
  PTR private_data;
  int (*read_memory_func)
    (bfd_vma memaddr, bfd_byte *myaddr, int length,
        struct disassemble_info *info);
  void (*memory_error_func)
    (int status, bfd_vma memaddr, struct disassemble_info *info);
  void (*print_address_func)
    (bfd_vma addr, struct disassemble_info *info);
    int (*print_insn)(bfd_vma addr, struct disassemble_info *info);
  int (* symbol_at_address_func)
    (bfd_vma addr, struct disassemble_info * info);
  const bfd_byte *buffer;
  bfd_vma buffer_vma;
  int buffer_length;
  int bytes_per_line;
  int bytes_per_chunk;
  enum bfd_endian display_endian;
  char insn_info_valid;
  char branch_delay_insns;
  char data_size;
  enum dis_insn_type insn_type;
  bfd_vma target;
  bfd_vma target2;
  char * disassembler_options;
  
 _Bool 
      show_opcodes;
  void *target_info;
  int cap_arch;
  int cap_mode;
  int cap_insn_unit;
  int cap_insn_split;
} disassemble_info;
typedef int (*disassembler_ftype) (bfd_vma, disassemble_info *);
int print_insn_tci(bfd_vma, disassemble_info*);
int print_insn_big_mips (bfd_vma, disassemble_info*);
int print_insn_little_mips (bfd_vma, disassemble_info*);
int print_insn_nanomips (bfd_vma, disassemble_info*);
int print_insn_m68k (bfd_vma, disassemble_info*);
int print_insn_z8001 (bfd_vma, disassemble_info*);
int print_insn_z8002 (bfd_vma, disassemble_info*);
int print_insn_h8300 (bfd_vma, disassemble_info*);
int print_insn_h8300h (bfd_vma, disassemble_info*);
int print_insn_h8300s (bfd_vma, disassemble_info*);
int print_insn_h8500 (bfd_vma, disassemble_info*);
int print_insn_arm_a64 (bfd_vma, disassemble_info*);
int print_insn_alpha (bfd_vma, disassemble_info*);
disassembler_ftype arc_get_disassembler (int, int);
int print_insn_sparc (bfd_vma, disassemble_info*);
int print_insn_big_a29k (bfd_vma, disassemble_info*);
int print_insn_little_a29k (bfd_vma, disassemble_info*);
int print_insn_i960 (bfd_vma, disassemble_info*);
int print_insn_sh (bfd_vma, disassemble_info*);
int print_insn_shl (bfd_vma, disassemble_info*);
int print_insn_hppa (bfd_vma, disassemble_info*);
int print_insn_m32r (bfd_vma, disassemble_info*);
int print_insn_m88k (bfd_vma, disassemble_info*);
int print_insn_mn10200 (bfd_vma, disassemble_info*);
int print_insn_mn10300 (bfd_vma, disassemble_info*);
int print_insn_ns32k (bfd_vma, disassemble_info*);
int print_insn_big_powerpc (bfd_vma, disassemble_info*);
int print_insn_little_powerpc (bfd_vma, disassemble_info*);
int print_insn_rs6000 (bfd_vma, disassemble_info*);
int print_insn_w65 (bfd_vma, disassemble_info*);
int print_insn_d10v (bfd_vma, disassemble_info*);
int print_insn_v850 (bfd_vma, disassemble_info*);
int print_insn_tic30 (bfd_vma, disassemble_info*);
int print_insn_microblaze (bfd_vma, disassemble_info*);
int print_insn_ia64 (bfd_vma, disassemble_info*);
int print_insn_xtensa (bfd_vma, disassemble_info*);
int print_insn_riscv32 (bfd_vma, disassemble_info*);
int print_insn_riscv64 (bfd_vma, disassemble_info*);
int print_insn_riscv128 (bfd_vma, disassemble_info*);
int print_insn_rx(bfd_vma, disassemble_info *);
int print_insn_hexagon(bfd_vma, disassemble_info *);
int print_insn_loongarch(bfd_vma, disassemble_info *);
_Bool 
    cap_disas_target(disassemble_info *info, uint64_t pc, size_t size);
_Bool 
    cap_disas_host(disassemble_info *info, const void *code, size_t size);
_Bool 
    cap_disas_monitor(disassemble_info *info, uint64_t pc, int count);
_Bool 
    cap_disas_plugin(disassemble_info *info, uint64_t pc, size_t size);
typedef 
       _Bool 
            bfd_boolean;
typedef struct MemTxAttrs {
    unsigned int unspecified:1;
    unsigned int secure:1;
    unsigned int space:2;
    unsigned int user:1;
    unsigned int memory:1;
    unsigned int requester_id:16;
    unsigned int pid:8;
} MemTxAttrs;
typedef uint32_t MemTxResult;
typedef struct CPUBreakpoint {
    vaddr pc;
    int flags;
    union { struct CPUBreakpoint *tqe_next; QTailQLink tqe_circ; } entry;
} CPUBreakpoint;
typedef struct CPUWatchpoint {
    vaddr vaddr;
    vaddr len;
    vaddr hitaddr;
    MemTxAttrs hitattrs;
    int flags;
    union { struct CPUWatchpoint *tqe_next; QTailQLink tqe_circ; } entry;
} CPUWatchpoint;
typedef enum MMUAccessType {
    MMU_DATA_LOAD = 0,
    MMU_DATA_STORE = 1,
    MMU_INST_FETCH = 2
} MMUAccessType;
typedef union CPUTLBEntry {
    struct {
        uint64_t addr_read;
        uint64_t addr_write;
        uint64_t addr_code;
        uintptr_t addend;
    };
    uint64_t addr_idx[(1 << 5) / sizeof(uint64_t)];
} CPUTLBEntry;
_Static_assert(!(sizeof(CPUTLBEntry) != (1 << 5)), "not expecting: " "sizeof(CPUTLBEntry) != (1 << CPU_TLB_ENTRY_BITS)");
typedef struct CPUTLBDescFast {
    uintptr_t mask;
    CPUTLBEntry *table;
} CPUTLBDescFast __attribute__((aligned(2 * sizeof(void *))));
typedef enum IoOperationType {
    IO_OPERATION_TYPE_READ,
    IO_OPERATION_TYPE_WRITE,
    IO_OPERATION_TYPE__MAX,
} IoOperationType;
extern const QEnumLookup IoOperationType_lookup;
typedef enum OnOffAuto {
    ON_OFF_AUTO_AUTO,
    ON_OFF_AUTO_ON,
    ON_OFF_AUTO_OFF,
    ON_OFF_AUTO__MAX,
} OnOffAuto;
extern const QEnumLookup OnOffAuto_lookup;
typedef enum OnOffSplit {
    ON_OFF_SPLIT_ON,
    ON_OFF_SPLIT_OFF,
    ON_OFF_SPLIT_SPLIT,
    ON_OFF_SPLIT__MAX,
} OnOffSplit;
extern const QEnumLookup OnOffSplit_lookup;
typedef struct StrOrNull StrOrNull;
typedef enum OffAutoPCIBAR {
    OFF_AUTO_PCIBAR_OFF,
    OFF_AUTO_PCIBAR_AUTO,
    OFF_AUTO_PCIBAR_BAR0,
    OFF_AUTO_PCIBAR_BAR1,
    OFF_AUTO_PCIBAR_BAR2,
    OFF_AUTO_PCIBAR_BAR3,
    OFF_AUTO_PCIBAR_BAR4,
    OFF_AUTO_PCIBAR_BAR5,
    OFF_AUTO_PCIBAR__MAX,
} OffAutoPCIBAR;
extern const QEnumLookup OffAutoPCIBAR_lookup;
typedef enum PCIELinkSpeed {
    PCIE_LINK_SPEED_2_5,
    PCIE_LINK_SPEED_5,
    PCIE_LINK_SPEED_8,
    PCIE_LINK_SPEED_16,
    PCIE_LINK_SPEED_32,
    PCIE_LINK_SPEED_64,
    PCIE_LINK_SPEED__MAX,
} PCIELinkSpeed;
extern const QEnumLookup PCIELinkSpeed_lookup;
typedef enum PCIELinkWidth {
    PCIE_LINK_WIDTH_1,
    PCIE_LINK_WIDTH_2,
    PCIE_LINK_WIDTH_4,
    PCIE_LINK_WIDTH_8,
    PCIE_LINK_WIDTH_12,
    PCIE_LINK_WIDTH_16,
    PCIE_LINK_WIDTH_32,
    PCIE_LINK_WIDTH__MAX,
} PCIELinkWidth;
extern const QEnumLookup PCIELinkWidth_lookup;
typedef enum HostMemPolicy {
    HOST_MEM_POLICY_DEFAULT,
    HOST_MEM_POLICY_PREFERRED,
    HOST_MEM_POLICY_BIND,
    HOST_MEM_POLICY_INTERLEAVE,
    HOST_MEM_POLICY__MAX,
} HostMemPolicy;
extern const QEnumLookup HostMemPolicy_lookup;
typedef enum NetFilterDirection {
    NET_FILTER_DIRECTION_ALL,
    NET_FILTER_DIRECTION_RX,
    NET_FILTER_DIRECTION_TX,
    NET_FILTER_DIRECTION__MAX,
} NetFilterDirection;
extern const QEnumLookup NetFilterDirection_lookup;
typedef enum GrabToggleKeys {
    GRAB_TOGGLE_KEYS_CTRL_CTRL,
    GRAB_TOGGLE_KEYS_ALT_ALT,
    GRAB_TOGGLE_KEYS_SHIFT_SHIFT,
    GRAB_TOGGLE_KEYS_META_META,
    GRAB_TOGGLE_KEYS_SCROLLLOCK,
    GRAB_TOGGLE_KEYS_CTRL_SCROLLLOCK,
    GRAB_TOGGLE_KEYS__MAX,
} GrabToggleKeys;
extern const QEnumLookup GrabToggleKeys_lookup;
typedef struct HumanReadableText HumanReadableText;
struct StrOrNull {
    QType type;
    union {
        char *s;
        QNull *n;
    } u;
};
void qapi_free_StrOrNull(StrOrNull *obj);
typedef StrOrNull *StrOrNull_autoptr; typedef GList *StrOrNull_listautoptr; typedef GSList *StrOrNull_slistautoptr; typedef GQueue *StrOrNull_queueautoptr;
struct HumanReadableText {
    char *human_readable_text;
};
void qapi_free_HumanReadableText(HumanReadableText *obj);
typedef HumanReadableText *HumanReadableText_autoptr; typedef GList *HumanReadableText_listautoptr; typedef GSList *HumanReadableText_slistautoptr; typedef GQueue *HumanReadableText_queueautoptr;
typedef enum S390CpuEntitlement {
    S390_CPU_ENTITLEMENT_AUTO,
    S390_CPU_ENTITLEMENT_LOW,
    S390_CPU_ENTITLEMENT_MEDIUM,
    S390_CPU_ENTITLEMENT_HIGH,
    S390_CPU_ENTITLEMENT__MAX,
} S390CpuEntitlement;
extern const QEnumLookup S390CpuEntitlement_lookup;
typedef enum CpuTopologyLevel {
    CPU_TOPOLOGY_LEVEL_THREAD,
    CPU_TOPOLOGY_LEVEL_CORE,
    CPU_TOPOLOGY_LEVEL_MODULE,
    CPU_TOPOLOGY_LEVEL_CLUSTER,
    CPU_TOPOLOGY_LEVEL_DIE,
    CPU_TOPOLOGY_LEVEL_SOCKET,
    CPU_TOPOLOGY_LEVEL_BOOK,
    CPU_TOPOLOGY_LEVEL_DRAWER,
    CPU_TOPOLOGY_LEVEL_DEFAULT,
    CPU_TOPOLOGY_LEVEL__MAX,
} CpuTopologyLevel;
extern const QEnumLookup CpuTopologyLevel_lookup;
typedef enum CacheLevelAndType {
    CACHE_LEVEL_AND_TYPE_L1D,
    CACHE_LEVEL_AND_TYPE_L1I,
    CACHE_LEVEL_AND_TYPE_L2,
    CACHE_LEVEL_AND_TYPE_L3,
    CACHE_LEVEL_AND_TYPE__MAX,
} CacheLevelAndType;
extern const QEnumLookup CacheLevelAndType_lookup;
typedef struct SmpCacheProperties SmpCacheProperties;
typedef struct SmpCachePropertiesList SmpCachePropertiesList;
typedef struct SmpCachePropertiesWrapper SmpCachePropertiesWrapper;
struct SmpCacheProperties {
    CacheLevelAndType cache;
    CpuTopologyLevel topology;
};
void qapi_free_SmpCacheProperties(SmpCacheProperties *obj);
typedef SmpCacheProperties *SmpCacheProperties_autoptr; typedef GList *SmpCacheProperties_listautoptr; typedef GSList *SmpCacheProperties_slistautoptr; typedef GQueue *SmpCacheProperties_queueautoptr;
struct SmpCachePropertiesList {
    SmpCachePropertiesList *next;
    SmpCacheProperties *value;
};
void qapi_free_SmpCachePropertiesList(SmpCachePropertiesList *obj);
typedef SmpCachePropertiesList *SmpCachePropertiesList_autoptr; typedef GList *SmpCachePropertiesList_listautoptr; typedef GSList *SmpCachePropertiesList_slistautoptr; typedef GQueue *SmpCachePropertiesList_queueautoptr;
struct SmpCachePropertiesWrapper {
    SmpCachePropertiesList *caches;
};
void qapi_free_SmpCachePropertiesWrapper(SmpCachePropertiesWrapper *obj);
typedef SmpCachePropertiesWrapper *SmpCachePropertiesWrapper_autoptr; typedef GList *SmpCachePropertiesWrapper_listautoptr; typedef GSList *SmpCachePropertiesWrapper_slistautoptr; typedef GQueue *SmpCachePropertiesWrapper_queueautoptr;
typedef enum SysEmuTarget {
    SYS_EMU_TARGET_AARCH64,
    SYS_EMU_TARGET_ALPHA,
    SYS_EMU_TARGET_ARM,
    SYS_EMU_TARGET_AVR,
    SYS_EMU_TARGET_HPPA,
    SYS_EMU_TARGET_I386,
    SYS_EMU_TARGET_LOONGARCH64,
    SYS_EMU_TARGET_M68K,
    SYS_EMU_TARGET_MICROBLAZE,
    SYS_EMU_TARGET_MICROBLAZEEL,
    SYS_EMU_TARGET_MIPS,
    SYS_EMU_TARGET_MIPS64,
    SYS_EMU_TARGET_MIPS64EL,
    SYS_EMU_TARGET_MIPSEL,
    SYS_EMU_TARGET_OR1K,
    SYS_EMU_TARGET_PPC,
    SYS_EMU_TARGET_PPC64,
    SYS_EMU_TARGET_RISCV32,
    SYS_EMU_TARGET_RISCV64,
    SYS_EMU_TARGET_RX,
    SYS_EMU_TARGET_S390X,
    SYS_EMU_TARGET_SH4,
    SYS_EMU_TARGET_SH4EB,
    SYS_EMU_TARGET_SPARC,
    SYS_EMU_TARGET_SPARC64,
    SYS_EMU_TARGET_TRICORE,
    SYS_EMU_TARGET_X86_64,
    SYS_EMU_TARGET_XTENSA,
    SYS_EMU_TARGET_XTENSAEB,
    SYS_EMU_TARGET__MAX,
} SysEmuTarget;
extern const QEnumLookup SysEmuTarget_lookup;
typedef enum S390CpuState {
    S390_CPU_STATE_UNINITIALIZED,
    S390_CPU_STATE_STOPPED,
    S390_CPU_STATE_CHECK_STOP,
    S390_CPU_STATE_OPERATING,
    S390_CPU_STATE_LOAD,
    S390_CPU_STATE__MAX,
} S390CpuState;
extern const QEnumLookup S390CpuState_lookup;
typedef struct CpuInfoS390 CpuInfoS390;
typedef struct q_obj_CpuInfoFast_base q_obj_CpuInfoFast_base;
typedef struct CpuInfoFast CpuInfoFast;
typedef struct CpuInfoFastList CpuInfoFastList;
typedef struct CompatProperty CompatProperty;
typedef struct CompatPropertyList CompatPropertyList;
typedef struct MachineInfo MachineInfo;
typedef struct q_obj_query_machines_arg q_obj_query_machines_arg;
typedef struct MachineInfoList MachineInfoList;
typedef struct CurrentMachineParams CurrentMachineParams;
typedef struct TargetInfo TargetInfo;
typedef struct UuidInfo UuidInfo;
typedef struct GuidInfo GuidInfo;
typedef enum LostTickPolicy {
    LOST_TICK_POLICY_DISCARD,
    LOST_TICK_POLICY_DELAY,
    LOST_TICK_POLICY_SLEW,
    LOST_TICK_POLICY__MAX,
} LostTickPolicy;
extern const QEnumLookup LostTickPolicy_lookup;
typedef struct KvmInfo KvmInfo;
typedef enum NumaOptionsType {
    NUMA_OPTIONS_TYPE_NODE,
    NUMA_OPTIONS_TYPE_DIST,
    NUMA_OPTIONS_TYPE_CPU,
    NUMA_OPTIONS_TYPE_HMAT_LB,
    NUMA_OPTIONS_TYPE_HMAT_CACHE,
    NUMA_OPTIONS_TYPE__MAX,
} NumaOptionsType;
extern const QEnumLookup NumaOptionsType_lookup;
typedef struct q_obj_NumaOptions_base q_obj_NumaOptions_base;
typedef struct NumaOptions NumaOptions;
typedef struct NumaNodeOptions NumaNodeOptions;
typedef struct NumaDistOptions NumaDistOptions;
typedef struct CXLFixedMemoryWindowOptions CXLFixedMemoryWindowOptions;
typedef struct CXLFixedMemoryWindowOptionsList CXLFixedMemoryWindowOptionsList;
typedef struct CXLFMWProperties CXLFMWProperties;
typedef enum X86CPURegister32 {
    X86_CPU_REGISTER32_EAX,
    X86_CPU_REGISTER32_EBX,
    X86_CPU_REGISTER32_ECX,
    X86_CPU_REGISTER32_EDX,
    X86_CPU_REGISTER32_ESP,
    X86_CPU_REGISTER32_EBP,
    X86_CPU_REGISTER32_ESI,
    X86_CPU_REGISTER32_EDI,
    X86_CPU_REGISTER32__MAX,
} X86CPURegister32;
extern const QEnumLookup X86CPURegister32_lookup;
typedef struct X86CPUFeatureWordInfo X86CPUFeatureWordInfo;
typedef struct X86CPUFeatureWordInfoList X86CPUFeatureWordInfoList;
typedef struct DummyForceArrays DummyForceArrays;
typedef struct NumaCpuOptions NumaCpuOptions;
typedef enum HmatLBMemoryHierarchy {
    HMAT_LB_MEMORY_HIERARCHY_MEMORY,
    HMAT_LB_MEMORY_HIERARCHY_FIRST_LEVEL,
    HMAT_LB_MEMORY_HIERARCHY_SECOND_LEVEL,
    HMAT_LB_MEMORY_HIERARCHY_THIRD_LEVEL,
    HMAT_LB_MEMORY_HIERARCHY__MAX,
} HmatLBMemoryHierarchy;
extern const QEnumLookup HmatLBMemoryHierarchy_lookup;
typedef enum HmatLBDataType {
    HMAT_LB_DATA_TYPE_ACCESS_LATENCY,
    HMAT_LB_DATA_TYPE_READ_LATENCY,
    HMAT_LB_DATA_TYPE_WRITE_LATENCY,
    HMAT_LB_DATA_TYPE_ACCESS_BANDWIDTH,
    HMAT_LB_DATA_TYPE_READ_BANDWIDTH,
    HMAT_LB_DATA_TYPE_WRITE_BANDWIDTH,
    HMAT_LB_DATA_TYPE__MAX,
} HmatLBDataType;
extern const QEnumLookup HmatLBDataType_lookup;
typedef struct NumaHmatLBOptions NumaHmatLBOptions;
typedef enum HmatCacheAssociativity {
    HMAT_CACHE_ASSOCIATIVITY_NONE,
    HMAT_CACHE_ASSOCIATIVITY_DIRECT,
    HMAT_CACHE_ASSOCIATIVITY_COMPLEX,
    HMAT_CACHE_ASSOCIATIVITY__MAX,
} HmatCacheAssociativity;
extern const QEnumLookup HmatCacheAssociativity_lookup;
typedef enum HmatCacheWritePolicy {
    HMAT_CACHE_WRITE_POLICY_NONE,
    HMAT_CACHE_WRITE_POLICY_WRITE_BACK,
    HMAT_CACHE_WRITE_POLICY_WRITE_THROUGH,
    HMAT_CACHE_WRITE_POLICY__MAX,
} HmatCacheWritePolicy;
extern const QEnumLookup HmatCacheWritePolicy_lookup;
typedef struct NumaHmatCacheOptions NumaHmatCacheOptions;
typedef struct q_obj_memsave_arg q_obj_memsave_arg;
typedef struct q_obj_pmemsave_arg q_obj_pmemsave_arg;
typedef struct Memdev Memdev;
typedef struct MemdevList MemdevList;
typedef struct CpuInstanceProperties CpuInstanceProperties;
typedef struct HotpluggableCPU HotpluggableCPU;
typedef struct HotpluggableCPUList HotpluggableCPUList;
typedef struct q_obj_balloon_arg q_obj_balloon_arg;
typedef struct BalloonInfo BalloonInfo;
typedef struct q_obj_BALLOON_CHANGE_arg q_obj_BALLOON_CHANGE_arg;
typedef struct HvBalloonInfo HvBalloonInfo;
typedef struct MemoryInfo MemoryInfo;
typedef struct PCDIMMDeviceInfo PCDIMMDeviceInfo;
typedef struct VirtioPMEMDeviceInfo VirtioPMEMDeviceInfo;
typedef struct VirtioMEMDeviceInfo VirtioMEMDeviceInfo;
typedef struct SgxEPCDeviceInfo SgxEPCDeviceInfo;
typedef struct HvBalloonDeviceInfo HvBalloonDeviceInfo;
typedef enum MemoryDeviceInfoKind {
    MEMORY_DEVICE_INFO_KIND_DIMM,
    MEMORY_DEVICE_INFO_KIND_NVDIMM,
    MEMORY_DEVICE_INFO_KIND_VIRTIO_PMEM,
    MEMORY_DEVICE_INFO_KIND_VIRTIO_MEM,
    MEMORY_DEVICE_INFO_KIND_SGX_EPC,
    MEMORY_DEVICE_INFO_KIND_HV_BALLOON,
    MEMORY_DEVICE_INFO_KIND__MAX,
} MemoryDeviceInfoKind;
extern const QEnumLookup MemoryDeviceInfoKind_lookup;
typedef struct PCDIMMDeviceInfoWrapper PCDIMMDeviceInfoWrapper;
typedef struct VirtioPMEMDeviceInfoWrapper VirtioPMEMDeviceInfoWrapper;
typedef struct VirtioMEMDeviceInfoWrapper VirtioMEMDeviceInfoWrapper;
typedef struct SgxEPCDeviceInfoWrapper SgxEPCDeviceInfoWrapper;
typedef struct HvBalloonDeviceInfoWrapper HvBalloonDeviceInfoWrapper;
typedef struct q_obj_MemoryDeviceInfo_base q_obj_MemoryDeviceInfo_base;
typedef struct MemoryDeviceInfo MemoryDeviceInfo;
typedef struct SgxEPC SgxEPC;
typedef struct SgxEPCList SgxEPCList;
typedef struct SgxEPCProperties SgxEPCProperties;
typedef struct MemoryDeviceInfoList MemoryDeviceInfoList;
typedef struct q_obj_MEMORY_DEVICE_SIZE_CHANGE_arg q_obj_MEMORY_DEVICE_SIZE_CHANGE_arg;
typedef struct BootConfiguration BootConfiguration;
typedef struct SMPConfiguration SMPConfiguration;
typedef enum SmbiosEntryPointType {
    SMBIOS_ENTRY_POINT_TYPE_32,
    SMBIOS_ENTRY_POINT_TYPE_64,
    SMBIOS_ENTRY_POINT_TYPE_AUTO,
    SMBIOS_ENTRY_POINT_TYPE__MAX,
} SmbiosEntryPointType;
extern const QEnumLookup SmbiosEntryPointType_lookup;
typedef struct MemorySizeConfiguration MemorySizeConfiguration;
typedef struct q_obj_dumpdtb_arg q_obj_dumpdtb_arg;
struct CpuInfoS390 {
    S390CpuState cpu_state;
    
   _Bool 
        has_dedicated;
    
   _Bool 
        dedicated;
    
   _Bool 
        has_entitlement;
    S390CpuEntitlement entitlement;
};
void qapi_free_CpuInfoS390(CpuInfoS390 *obj);
typedef CpuInfoS390 *CpuInfoS390_autoptr; typedef GList *CpuInfoS390_listautoptr; typedef GSList *CpuInfoS390_slistautoptr; typedef GQueue *CpuInfoS390_queueautoptr;
struct q_obj_CpuInfoFast_base {
    int64_t cpu_index;
    char *qom_path;
    int64_t thread_id;
    CpuInstanceProperties *props;
    SysEmuTarget target;
};
struct CpuInfoFast {
    int64_t cpu_index;
    char *qom_path;
    int64_t thread_id;
    CpuInstanceProperties *props;
    SysEmuTarget target;
    union {
        CpuInfoS390 s390x;
    } u;
};
void qapi_free_CpuInfoFast(CpuInfoFast *obj);
typedef CpuInfoFast *CpuInfoFast_autoptr; typedef GList *CpuInfoFast_listautoptr; typedef GSList *CpuInfoFast_slistautoptr; typedef GQueue *CpuInfoFast_queueautoptr;
struct CpuInfoFastList {
    CpuInfoFastList *next;
    CpuInfoFast *value;
};
void qapi_free_CpuInfoFastList(CpuInfoFastList *obj);
typedef CpuInfoFastList *CpuInfoFastList_autoptr; typedef GList *CpuInfoFastList_listautoptr; typedef GSList *CpuInfoFastList_slistautoptr; typedef GQueue *CpuInfoFastList_queueautoptr;
struct CompatProperty {
    char *qom_type;
    char *property;
    char *value;
};
void qapi_free_CompatProperty(CompatProperty *obj);
typedef CompatProperty *CompatProperty_autoptr; typedef GList *CompatProperty_listautoptr; typedef GSList *CompatProperty_slistautoptr; typedef GQueue *CompatProperty_queueautoptr;
struct CompatPropertyList {
    CompatPropertyList *next;
    CompatProperty *value;
};
void qapi_free_CompatPropertyList(CompatPropertyList *obj);
typedef CompatPropertyList *CompatPropertyList_autoptr; typedef GList *CompatPropertyList_listautoptr; typedef GSList *CompatPropertyList_slistautoptr; typedef GQueue *CompatPropertyList_queueautoptr;
struct MachineInfo {
    char *name;
    char *alias;
    
   _Bool 
        has_is_default;
    
   _Bool 
        is_default;
    int64_t cpu_max;
    
   _Bool 
        hotpluggable_cpus;
    
   _Bool 
        numa_mem_supported;
    
   _Bool 
        deprecated;
    char *default_cpu_type;
    char *default_ram_id;
    
   _Bool 
        acpi;
    
   _Bool 
        has_compat_props;
    CompatPropertyList *compat_props;
};
void qapi_free_MachineInfo(MachineInfo *obj);
typedef MachineInfo *MachineInfo_autoptr; typedef GList *MachineInfo_listautoptr; typedef GSList *MachineInfo_slistautoptr; typedef GQueue *MachineInfo_queueautoptr;
struct q_obj_query_machines_arg {
    
   _Bool 
        has_compat_props;
    
   _Bool 
        compat_props;
};
struct MachineInfoList {
    MachineInfoList *next;
    MachineInfo *value;
};
void qapi_free_MachineInfoList(MachineInfoList *obj);
typedef MachineInfoList *MachineInfoList_autoptr; typedef GList *MachineInfoList_listautoptr; typedef GSList *MachineInfoList_slistautoptr; typedef GQueue *MachineInfoList_queueautoptr;
struct CurrentMachineParams {
    
   _Bool 
        wakeup_suspend_support;
};
void qapi_free_CurrentMachineParams(CurrentMachineParams *obj);
typedef CurrentMachineParams *CurrentMachineParams_autoptr; typedef GList *CurrentMachineParams_listautoptr; typedef GSList *CurrentMachineParams_slistautoptr; typedef GQueue *CurrentMachineParams_queueautoptr;
struct TargetInfo {
    SysEmuTarget arch;
};
void qapi_free_TargetInfo(TargetInfo *obj);
typedef TargetInfo *TargetInfo_autoptr; typedef GList *TargetInfo_listautoptr; typedef GSList *TargetInfo_slistautoptr; typedef GQueue *TargetInfo_queueautoptr;
struct UuidInfo {
    char *UUID;
};
void qapi_free_UuidInfo(UuidInfo *obj);
typedef UuidInfo *UuidInfo_autoptr; typedef GList *UuidInfo_listautoptr; typedef GSList *UuidInfo_slistautoptr; typedef GQueue *UuidInfo_queueautoptr;
struct GuidInfo {
    char *guid;
};
void qapi_free_GuidInfo(GuidInfo *obj);
typedef GuidInfo *GuidInfo_autoptr; typedef GList *GuidInfo_listautoptr; typedef GSList *GuidInfo_slistautoptr; typedef GQueue *GuidInfo_queueautoptr;
struct KvmInfo {
    
   _Bool 
        enabled;
    
   _Bool 
        present;
};
void qapi_free_KvmInfo(KvmInfo *obj);
typedef KvmInfo *KvmInfo_autoptr; typedef GList *KvmInfo_listautoptr; typedef GSList *KvmInfo_slistautoptr; typedef GQueue *KvmInfo_queueautoptr;
struct q_obj_NumaOptions_base {
    NumaOptionsType type;
};
struct NumaNodeOptions {
    
   _Bool 
        has_nodeid;
    uint16_t nodeid;
    
   _Bool 
        has_cpus;
    uint16List *cpus;
    
   _Bool 
        has_mem;
    uint64_t mem;
    char *memdev;
    
   _Bool 
        has_initiator;
    uint16_t initiator;
};
struct NumaDistOptions {
    uint16_t src;
    uint16_t dst;
    uint8_t val;
};
struct NumaCpuOptions {
    
   _Bool 
        has_node_id;
    int64_t node_id;
    
   _Bool 
        has_drawer_id;
    int64_t drawer_id;
    
   _Bool 
        has_book_id;
    int64_t book_id;
    
   _Bool 
        has_socket_id;
    int64_t socket_id;
    
   _Bool 
        has_die_id;
    int64_t die_id;
    
   _Bool 
        has_cluster_id;
    int64_t cluster_id;
    
   _Bool 
        has_module_id;
    int64_t module_id;
    
   _Bool 
        has_core_id;
    int64_t core_id;
    
   _Bool 
        has_thread_id;
    int64_t thread_id;
};
struct NumaHmatLBOptions {
    uint16_t initiator;
    uint16_t target;
    HmatLBMemoryHierarchy hierarchy;
    HmatLBDataType data_type;
    
   _Bool 
        has_latency;
    uint64_t latency;
    
   _Bool 
        has_bandwidth;
    uint64_t bandwidth;
};
struct NumaHmatCacheOptions {
    uint32_t node_id;
    uint64_t size;
    uint8_t level;
    HmatCacheAssociativity associativity;
    HmatCacheWritePolicy policy;
    uint16_t line;
};
struct NumaOptions {
    NumaOptionsType type;
    union {
        NumaNodeOptions node;
        NumaDistOptions dist;
        NumaCpuOptions cpu;
        NumaHmatLBOptions hmat_lb;
        NumaHmatCacheOptions hmat_cache;
    } u;
};
void qapi_free_NumaOptions(NumaOptions *obj);
typedef NumaOptions *NumaOptions_autoptr; typedef GList *NumaOptions_listautoptr; typedef GSList *NumaOptions_slistautoptr; typedef GQueue *NumaOptions_queueautoptr;
void qapi_free_NumaNodeOptions(NumaNodeOptions *obj);
typedef NumaNodeOptions *NumaNodeOptions_autoptr; typedef GList *NumaNodeOptions_listautoptr; typedef GSList *NumaNodeOptions_slistautoptr; typedef GQueue *NumaNodeOptions_queueautoptr;
void qapi_free_NumaDistOptions(NumaDistOptions *obj);
typedef NumaDistOptions *NumaDistOptions_autoptr; typedef GList *NumaDistOptions_listautoptr; typedef GSList *NumaDistOptions_slistautoptr; typedef GQueue *NumaDistOptions_queueautoptr;
struct CXLFixedMemoryWindowOptions {
    uint64_t size;
    
   _Bool 
        has_interleave_granularity;
    uint64_t interleave_granularity;
    strList *targets;
};
void qapi_free_CXLFixedMemoryWindowOptions(CXLFixedMemoryWindowOptions *obj);
typedef CXLFixedMemoryWindowOptions *CXLFixedMemoryWindowOptions_autoptr; typedef GList *CXLFixedMemoryWindowOptions_listautoptr; typedef GSList *CXLFixedMemoryWindowOptions_slistautoptr; typedef GQueue *CXLFixedMemoryWindowOptions_queueautoptr;
struct CXLFixedMemoryWindowOptionsList {
    CXLFixedMemoryWindowOptionsList *next;
    CXLFixedMemoryWindowOptions *value;
};
void qapi_free_CXLFixedMemoryWindowOptionsList(CXLFixedMemoryWindowOptionsList *obj);
typedef CXLFixedMemoryWindowOptionsList *CXLFixedMemoryWindowOptionsList_autoptr; typedef GList *CXLFixedMemoryWindowOptionsList_listautoptr; typedef GSList *CXLFixedMemoryWindowOptionsList_slistautoptr; typedef GQueue *CXLFixedMemoryWindowOptionsList_queueautoptr;
struct CXLFMWProperties {
    CXLFixedMemoryWindowOptionsList *cxl_fmw;
};
void qapi_free_CXLFMWProperties(CXLFMWProperties *obj);
typedef CXLFMWProperties *CXLFMWProperties_autoptr; typedef GList *CXLFMWProperties_listautoptr; typedef GSList *CXLFMWProperties_slistautoptr; typedef GQueue *CXLFMWProperties_queueautoptr;
struct X86CPUFeatureWordInfo {
    int64_t cpuid_input_eax;
    
   _Bool 
        has_cpuid_input_ecx;
    int64_t cpuid_input_ecx;
    X86CPURegister32 cpuid_register;
    int64_t features;
};
void qapi_free_X86CPUFeatureWordInfo(X86CPUFeatureWordInfo *obj);
typedef X86CPUFeatureWordInfo *X86CPUFeatureWordInfo_autoptr; typedef GList *X86CPUFeatureWordInfo_listautoptr; typedef GSList *X86CPUFeatureWordInfo_slistautoptr; typedef GQueue *X86CPUFeatureWordInfo_queueautoptr;
struct X86CPUFeatureWordInfoList {
    X86CPUFeatureWordInfoList *next;
    X86CPUFeatureWordInfo *value;
};
void qapi_free_X86CPUFeatureWordInfoList(X86CPUFeatureWordInfoList *obj);
typedef X86CPUFeatureWordInfoList *X86CPUFeatureWordInfoList_autoptr; typedef GList *X86CPUFeatureWordInfoList_listautoptr; typedef GSList *X86CPUFeatureWordInfoList_slistautoptr; typedef GQueue *X86CPUFeatureWordInfoList_queueautoptr;
struct DummyForceArrays {
    X86CPUFeatureWordInfoList *unused;
};
void qapi_free_DummyForceArrays(DummyForceArrays *obj);
typedef DummyForceArrays *DummyForceArrays_autoptr; typedef GList *DummyForceArrays_listautoptr; typedef GSList *DummyForceArrays_slistautoptr; typedef GQueue *DummyForceArrays_queueautoptr;
void qapi_free_NumaCpuOptions(NumaCpuOptions *obj);
typedef NumaCpuOptions *NumaCpuOptions_autoptr; typedef GList *NumaCpuOptions_listautoptr; typedef GSList *NumaCpuOptions_slistautoptr; typedef GQueue *NumaCpuOptions_queueautoptr;
void qapi_free_NumaHmatLBOptions(NumaHmatLBOptions *obj);
typedef NumaHmatLBOptions *NumaHmatLBOptions_autoptr; typedef GList *NumaHmatLBOptions_listautoptr; typedef GSList *NumaHmatLBOptions_slistautoptr; typedef GQueue *NumaHmatLBOptions_queueautoptr;
void qapi_free_NumaHmatCacheOptions(NumaHmatCacheOptions *obj);
typedef NumaHmatCacheOptions *NumaHmatCacheOptions_autoptr; typedef GList *NumaHmatCacheOptions_listautoptr; typedef GSList *NumaHmatCacheOptions_slistautoptr; typedef GQueue *NumaHmatCacheOptions_queueautoptr;
struct q_obj_memsave_arg {
    uint64_t val;
    uint64_t size;
    char *filename;
    
   _Bool 
        has_cpu_index;
    int64_t cpu_index;
};
struct q_obj_pmemsave_arg {
    uint64_t val;
    uint64_t size;
    char *filename;
};
struct Memdev {
    char *id;
    uint64_t size;
    
   _Bool 
        merge;
    
   _Bool 
        dump;
    
   _Bool 
        prealloc;
    
   _Bool 
        share;
    
   _Bool 
        has_reserve;
    
   _Bool 
        reserve;
    uint16List *host_nodes;
    HostMemPolicy policy;
};
void qapi_free_Memdev(Memdev *obj);
typedef Memdev *Memdev_autoptr; typedef GList *Memdev_listautoptr; typedef GSList *Memdev_slistautoptr; typedef GQueue *Memdev_queueautoptr;
struct MemdevList {
    MemdevList *next;
    Memdev *value;
};
void qapi_free_MemdevList(MemdevList *obj);
typedef MemdevList *MemdevList_autoptr; typedef GList *MemdevList_listautoptr; typedef GSList *MemdevList_slistautoptr; typedef GQueue *MemdevList_queueautoptr;
struct CpuInstanceProperties {
    
   _Bool 
        has_node_id;
    int64_t node_id;
    
   _Bool 
        has_drawer_id;
    int64_t drawer_id;
    
   _Bool 
        has_book_id;
    int64_t book_id;
    
   _Bool 
        has_socket_id;
    int64_t socket_id;
    
   _Bool 
        has_die_id;
    int64_t die_id;
    
   _Bool 
        has_cluster_id;
    int64_t cluster_id;
    
   _Bool 
        has_module_id;
    int64_t module_id;
    
   _Bool 
        has_core_id;
    int64_t core_id;
    
   _Bool 
        has_thread_id;
    int64_t thread_id;
};
void qapi_free_CpuInstanceProperties(CpuInstanceProperties *obj);
typedef CpuInstanceProperties *CpuInstanceProperties_autoptr; typedef GList *CpuInstanceProperties_listautoptr; typedef GSList *CpuInstanceProperties_slistautoptr; typedef GQueue *CpuInstanceProperties_queueautoptr;
struct HotpluggableCPU {
    char *type;
    int64_t vcpus_count;
    CpuInstanceProperties *props;
    char *qom_path;
};
void qapi_free_HotpluggableCPU(HotpluggableCPU *obj);
typedef HotpluggableCPU *HotpluggableCPU_autoptr; typedef GList *HotpluggableCPU_listautoptr; typedef GSList *HotpluggableCPU_slistautoptr; typedef GQueue *HotpluggableCPU_queueautoptr;
struct HotpluggableCPUList {
    HotpluggableCPUList *next;
    HotpluggableCPU *value;
};
void qapi_free_HotpluggableCPUList(HotpluggableCPUList *obj);
typedef HotpluggableCPUList *HotpluggableCPUList_autoptr; typedef GList *HotpluggableCPUList_listautoptr; typedef GSList *HotpluggableCPUList_slistautoptr; typedef GQueue *HotpluggableCPUList_queueautoptr;
struct q_obj_balloon_arg {
    int64_t value;
};
struct BalloonInfo {
    int64_t actual;
};
void qapi_free_BalloonInfo(BalloonInfo *obj);
typedef BalloonInfo *BalloonInfo_autoptr; typedef GList *BalloonInfo_listautoptr; typedef GSList *BalloonInfo_slistautoptr; typedef GQueue *BalloonInfo_queueautoptr;
struct q_obj_BALLOON_CHANGE_arg {
    int64_t actual;
};
struct HvBalloonInfo {
    uint64_t committed;
    uint64_t available;
};
void qapi_free_HvBalloonInfo(HvBalloonInfo *obj);
typedef HvBalloonInfo *HvBalloonInfo_autoptr; typedef GList *HvBalloonInfo_listautoptr; typedef GSList *HvBalloonInfo_slistautoptr; typedef GQueue *HvBalloonInfo_queueautoptr;
struct MemoryInfo {
    uint64_t base_memory;
    
   _Bool 
        has_plugged_memory;
    uint64_t plugged_memory;
};
void qapi_free_MemoryInfo(MemoryInfo *obj);
typedef MemoryInfo *MemoryInfo_autoptr; typedef GList *MemoryInfo_listautoptr; typedef GSList *MemoryInfo_slistautoptr; typedef GQueue *MemoryInfo_queueautoptr;
struct PCDIMMDeviceInfo {
    char *id;
    int64_t addr;
    int64_t size;
    int64_t slot;
    int64_t node;
    char *memdev;
    
   _Bool 
        hotplugged;
    
   _Bool 
        hotpluggable;
};
void qapi_free_PCDIMMDeviceInfo(PCDIMMDeviceInfo *obj);
typedef PCDIMMDeviceInfo *PCDIMMDeviceInfo_autoptr; typedef GList *PCDIMMDeviceInfo_listautoptr; typedef GSList *PCDIMMDeviceInfo_slistautoptr; typedef GQueue *PCDIMMDeviceInfo_queueautoptr;
struct VirtioPMEMDeviceInfo {
    char *id;
    uint64_t memaddr;
    uint64_t size;
    char *memdev;
};
void qapi_free_VirtioPMEMDeviceInfo(VirtioPMEMDeviceInfo *obj);
typedef VirtioPMEMDeviceInfo *VirtioPMEMDeviceInfo_autoptr; typedef GList *VirtioPMEMDeviceInfo_listautoptr; typedef GSList *VirtioPMEMDeviceInfo_slistautoptr; typedef GQueue *VirtioPMEMDeviceInfo_queueautoptr;
struct VirtioMEMDeviceInfo {
    char *id;
    uint64_t memaddr;
    uint64_t requested_size;
    uint64_t size;
    uint64_t max_size;
    uint64_t block_size;
    int64_t node;
    char *memdev;
};
void qapi_free_VirtioMEMDeviceInfo(VirtioMEMDeviceInfo *obj);
typedef VirtioMEMDeviceInfo *VirtioMEMDeviceInfo_autoptr; typedef GList *VirtioMEMDeviceInfo_listautoptr; typedef GSList *VirtioMEMDeviceInfo_slistautoptr; typedef GQueue *VirtioMEMDeviceInfo_queueautoptr;
struct SgxEPCDeviceInfo {
    char *id;
    uint64_t memaddr;
    uint64_t size;
    int64_t node;
    char *memdev;
};
void qapi_free_SgxEPCDeviceInfo(SgxEPCDeviceInfo *obj);
typedef SgxEPCDeviceInfo *SgxEPCDeviceInfo_autoptr; typedef GList *SgxEPCDeviceInfo_listautoptr; typedef GSList *SgxEPCDeviceInfo_slistautoptr; typedef GQueue *SgxEPCDeviceInfo_queueautoptr;
struct HvBalloonDeviceInfo {
    char *id;
    
   _Bool 
        has_memaddr;
    uint64_t memaddr;
    uint64_t max_size;
    char *memdev;
};
void qapi_free_HvBalloonDeviceInfo(HvBalloonDeviceInfo *obj);
typedef HvBalloonDeviceInfo *HvBalloonDeviceInfo_autoptr; typedef GList *HvBalloonDeviceInfo_listautoptr; typedef GSList *HvBalloonDeviceInfo_slistautoptr; typedef GQueue *HvBalloonDeviceInfo_queueautoptr;
struct PCDIMMDeviceInfoWrapper {
    PCDIMMDeviceInfo *data;
};
void qapi_free_PCDIMMDeviceInfoWrapper(PCDIMMDeviceInfoWrapper *obj);
typedef PCDIMMDeviceInfoWrapper *PCDIMMDeviceInfoWrapper_autoptr; typedef GList *PCDIMMDeviceInfoWrapper_listautoptr; typedef GSList *PCDIMMDeviceInfoWrapper_slistautoptr; typedef GQueue *PCDIMMDeviceInfoWrapper_queueautoptr;
struct VirtioPMEMDeviceInfoWrapper {
    VirtioPMEMDeviceInfo *data;
};
void qapi_free_VirtioPMEMDeviceInfoWrapper(VirtioPMEMDeviceInfoWrapper *obj);
typedef VirtioPMEMDeviceInfoWrapper *VirtioPMEMDeviceInfoWrapper_autoptr; typedef GList *VirtioPMEMDeviceInfoWrapper_listautoptr; typedef GSList *VirtioPMEMDeviceInfoWrapper_slistautoptr; typedef GQueue *VirtioPMEMDeviceInfoWrapper_queueautoptr;
struct VirtioMEMDeviceInfoWrapper {
    VirtioMEMDeviceInfo *data;
};
void qapi_free_VirtioMEMDeviceInfoWrapper(VirtioMEMDeviceInfoWrapper *obj);
typedef VirtioMEMDeviceInfoWrapper *VirtioMEMDeviceInfoWrapper_autoptr; typedef GList *VirtioMEMDeviceInfoWrapper_listautoptr; typedef GSList *VirtioMEMDeviceInfoWrapper_slistautoptr; typedef GQueue *VirtioMEMDeviceInfoWrapper_queueautoptr;
struct SgxEPCDeviceInfoWrapper {
    SgxEPCDeviceInfo *data;
};
void qapi_free_SgxEPCDeviceInfoWrapper(SgxEPCDeviceInfoWrapper *obj);
typedef SgxEPCDeviceInfoWrapper *SgxEPCDeviceInfoWrapper_autoptr; typedef GList *SgxEPCDeviceInfoWrapper_listautoptr; typedef GSList *SgxEPCDeviceInfoWrapper_slistautoptr; typedef GQueue *SgxEPCDeviceInfoWrapper_queueautoptr;
struct HvBalloonDeviceInfoWrapper {
    HvBalloonDeviceInfo *data;
};
void qapi_free_HvBalloonDeviceInfoWrapper(HvBalloonDeviceInfoWrapper *obj);
typedef HvBalloonDeviceInfoWrapper *HvBalloonDeviceInfoWrapper_autoptr; typedef GList *HvBalloonDeviceInfoWrapper_listautoptr; typedef GSList *HvBalloonDeviceInfoWrapper_slistautoptr; typedef GQueue *HvBalloonDeviceInfoWrapper_queueautoptr;
struct q_obj_MemoryDeviceInfo_base {
    MemoryDeviceInfoKind type;
};
struct MemoryDeviceInfo {
    MemoryDeviceInfoKind type;
    union {
        PCDIMMDeviceInfoWrapper dimm;
        PCDIMMDeviceInfoWrapper nvdimm;
        VirtioPMEMDeviceInfoWrapper virtio_pmem;
        VirtioMEMDeviceInfoWrapper virtio_mem;
        SgxEPCDeviceInfoWrapper sgx_epc;
        HvBalloonDeviceInfoWrapper hv_balloon;
    } u;
};
void qapi_free_MemoryDeviceInfo(MemoryDeviceInfo *obj);
typedef MemoryDeviceInfo *MemoryDeviceInfo_autoptr; typedef GList *MemoryDeviceInfo_listautoptr; typedef GSList *MemoryDeviceInfo_slistautoptr; typedef GQueue *MemoryDeviceInfo_queueautoptr;
struct SgxEPC {
    char *memdev;
    int64_t node;
};
void qapi_free_SgxEPC(SgxEPC *obj);
typedef SgxEPC *SgxEPC_autoptr; typedef GList *SgxEPC_listautoptr; typedef GSList *SgxEPC_slistautoptr; typedef GQueue *SgxEPC_queueautoptr;
struct SgxEPCList {
    SgxEPCList *next;
    SgxEPC *value;
};
void qapi_free_SgxEPCList(SgxEPCList *obj);
typedef SgxEPCList *SgxEPCList_autoptr; typedef GList *SgxEPCList_listautoptr; typedef GSList *SgxEPCList_slistautoptr; typedef GQueue *SgxEPCList_queueautoptr;
struct SgxEPCProperties {
    SgxEPCList *sgx_epc;
};
void qapi_free_SgxEPCProperties(SgxEPCProperties *obj);
typedef SgxEPCProperties *SgxEPCProperties_autoptr; typedef GList *SgxEPCProperties_listautoptr; typedef GSList *SgxEPCProperties_slistautoptr; typedef GQueue *SgxEPCProperties_queueautoptr;
struct MemoryDeviceInfoList {
    MemoryDeviceInfoList *next;
    MemoryDeviceInfo *value;
};
void qapi_free_MemoryDeviceInfoList(MemoryDeviceInfoList *obj);
typedef MemoryDeviceInfoList *MemoryDeviceInfoList_autoptr; typedef GList *MemoryDeviceInfoList_listautoptr; typedef GSList *MemoryDeviceInfoList_slistautoptr; typedef GQueue *MemoryDeviceInfoList_queueautoptr;
struct q_obj_MEMORY_DEVICE_SIZE_CHANGE_arg {
    char *id;
    uint64_t size;
    char *qom_path;
};
struct BootConfiguration {
    char *order;
    char *once;
    
   _Bool 
        has_menu;
    
   _Bool 
        menu;
    char *splash;
    
   _Bool 
        has_splash_time;
    int64_t splash_time;
    
   _Bool 
        has_reboot_timeout;
    int64_t reboot_timeout;
    
   _Bool 
        has_strict;
    
   _Bool 
        strict;
};
void qapi_free_BootConfiguration(BootConfiguration *obj);
typedef BootConfiguration *BootConfiguration_autoptr; typedef GList *BootConfiguration_listautoptr; typedef GSList *BootConfiguration_slistautoptr; typedef GQueue *BootConfiguration_queueautoptr;
struct SMPConfiguration {
    
   _Bool 
        has_cpus;
    int64_t cpus;
    
   _Bool 
        has_drawers;
    int64_t drawers;
    
   _Bool 
        has_books;
    int64_t books;
    
   _Bool 
        has_sockets;
    int64_t sockets;
    
   _Bool 
        has_dies;
    int64_t dies;
    
   _Bool 
        has_clusters;
    int64_t clusters;
    
   _Bool 
        has_modules;
    int64_t modules;
    
   _Bool 
        has_cores;
    int64_t cores;
    
   _Bool 
        has_threads;
    int64_t threads;
    
   _Bool 
        has_maxcpus;
    int64_t maxcpus;
};
void qapi_free_SMPConfiguration(SMPConfiguration *obj);
typedef SMPConfiguration *SMPConfiguration_autoptr; typedef GList *SMPConfiguration_listautoptr; typedef GSList *SMPConfiguration_slistautoptr; typedef GQueue *SMPConfiguration_queueautoptr;
struct MemorySizeConfiguration {
    
   _Bool 
        has_size;
    uint64_t size;
    
   _Bool 
        has_max_size;
    uint64_t max_size;
    
   _Bool 
        has_slots;
    uint64_t slots;
};
void qapi_free_MemorySizeConfiguration(MemorySizeConfiguration *obj);
typedef MemorySizeConfiguration *MemorySizeConfiguration_autoptr; typedef GList *MemorySizeConfiguration_listautoptr; typedef GSList *MemorySizeConfiguration_slistautoptr; typedef GQueue *MemorySizeConfiguration_queueautoptr;
struct q_obj_dumpdtb_arg {
    char *filename;
};
typedef enum RunState {
    RUN_STATE_DEBUG,
    RUN_STATE_INMIGRATE,
    RUN_STATE_INTERNAL_ERROR,
    RUN_STATE_IO_ERROR,
    RUN_STATE_PAUSED,
    RUN_STATE_POSTMIGRATE,
    RUN_STATE_PRELAUNCH,
    RUN_STATE_FINISH_MIGRATE,
    RUN_STATE_RESTORE_VM,
    RUN_STATE_RUNNING,
    RUN_STATE_SAVE_VM,
    RUN_STATE_SHUTDOWN,
    RUN_STATE_SUSPENDED,
    RUN_STATE_WATCHDOG,
    RUN_STATE_GUEST_PANICKED,
    RUN_STATE_COLO,
    RUN_STATE__MAX,
} RunState;
extern const QEnumLookup RunState_lookup;
typedef enum ShutdownCause {
    SHUTDOWN_CAUSE_NONE,
    SHUTDOWN_CAUSE_HOST_ERROR,
    SHUTDOWN_CAUSE_HOST_QMP_QUIT,
    SHUTDOWN_CAUSE_HOST_QMP_SYSTEM_RESET,
    SHUTDOWN_CAUSE_HOST_SIGNAL,
    SHUTDOWN_CAUSE_HOST_UI,
    SHUTDOWN_CAUSE_GUEST_SHUTDOWN,
    SHUTDOWN_CAUSE_GUEST_RESET,
    SHUTDOWN_CAUSE_GUEST_PANIC,
    SHUTDOWN_CAUSE_SUBSYSTEM_RESET,
    SHUTDOWN_CAUSE_SNAPSHOT_LOAD,
    SHUTDOWN_CAUSE__MAX,
} ShutdownCause;
extern const QEnumLookup ShutdownCause_lookup;
typedef struct StatusInfo StatusInfo;
typedef struct q_obj_SHUTDOWN_arg q_obj_SHUTDOWN_arg;
typedef struct q_obj_RESET_arg q_obj_RESET_arg;
typedef struct q_obj_WATCHDOG_arg q_obj_WATCHDOG_arg;
typedef enum WatchdogAction {
    WATCHDOG_ACTION_RESET,
    WATCHDOG_ACTION_SHUTDOWN,
    WATCHDOG_ACTION_POWEROFF,
    WATCHDOG_ACTION_PAUSE,
    WATCHDOG_ACTION_DEBUG,
    WATCHDOG_ACTION_NONE,
    WATCHDOG_ACTION_INJECT_NMI,
    WATCHDOG_ACTION__MAX,
} WatchdogAction;
extern const QEnumLookup WatchdogAction_lookup;
typedef enum RebootAction {
    REBOOT_ACTION_RESET,
    REBOOT_ACTION_SHUTDOWN,
    REBOOT_ACTION__MAX,
} RebootAction;
extern const QEnumLookup RebootAction_lookup;
typedef enum ShutdownAction {
    SHUTDOWN_ACTION_POWEROFF,
    SHUTDOWN_ACTION_PAUSE,
    SHUTDOWN_ACTION__MAX,
} ShutdownAction;
extern const QEnumLookup ShutdownAction_lookup;
typedef enum PanicAction {
    PANIC_ACTION_PAUSE,
    PANIC_ACTION_SHUTDOWN,
    PANIC_ACTION_EXIT_FAILURE,
    PANIC_ACTION_NONE,
    PANIC_ACTION__MAX,
} PanicAction;
extern const QEnumLookup PanicAction_lookup;
typedef struct q_obj_watchdog_set_action_arg q_obj_watchdog_set_action_arg;
typedef struct q_obj_set_action_arg q_obj_set_action_arg;
typedef struct q_obj_GUEST_PANICKED_arg q_obj_GUEST_PANICKED_arg;
typedef struct q_obj_GUEST_CRASHLOADED_arg q_obj_GUEST_CRASHLOADED_arg;
typedef enum GuestPanicAction {
    GUEST_PANIC_ACTION_PAUSE,
    GUEST_PANIC_ACTION_POWEROFF,
    GUEST_PANIC_ACTION_RUN,
    GUEST_PANIC_ACTION__MAX,
} GuestPanicAction;
extern const QEnumLookup GuestPanicAction_lookup;
typedef enum GuestPanicInformationType {
    GUEST_PANIC_INFORMATION_TYPE_HYPER_V,
    GUEST_PANIC_INFORMATION_TYPE_S390,
    GUEST_PANIC_INFORMATION_TYPE__MAX,
} GuestPanicInformationType;
extern const QEnumLookup GuestPanicInformationType_lookup;
typedef struct q_obj_GuestPanicInformation_base q_obj_GuestPanicInformation_base;
typedef struct GuestPanicInformation GuestPanicInformation;
typedef struct GuestPanicInformationHyperV GuestPanicInformationHyperV;
typedef enum S390CrashReason {
    S390_CRASH_REASON_UNKNOWN,
    S390_CRASH_REASON_DISABLED_WAIT,
    S390_CRASH_REASON_EXTINT_LOOP,
    S390_CRASH_REASON_PGMINT_LOOP,
    S390_CRASH_REASON_OPINT_LOOP,
    S390_CRASH_REASON__MAX,
} S390CrashReason;
extern const QEnumLookup S390CrashReason_lookup;
typedef struct GuestPanicInformationS390 GuestPanicInformationS390;
typedef struct q_obj_MEMORY_FAILURE_arg q_obj_MEMORY_FAILURE_arg;
typedef enum MemoryFailureRecipient {
    MEMORY_FAILURE_RECIPIENT_HYPERVISOR,
    MEMORY_FAILURE_RECIPIENT_GUEST,
    MEMORY_FAILURE_RECIPIENT__MAX,
} MemoryFailureRecipient;
extern const QEnumLookup MemoryFailureRecipient_lookup;
typedef enum MemoryFailureAction {
    MEMORY_FAILURE_ACTION_IGNORE,
    MEMORY_FAILURE_ACTION_INJECT,
    MEMORY_FAILURE_ACTION_FATAL,
    MEMORY_FAILURE_ACTION_RESET,
    MEMORY_FAILURE_ACTION__MAX,
} MemoryFailureAction;
extern const QEnumLookup MemoryFailureAction_lookup;
typedef struct MemoryFailureFlags MemoryFailureFlags;
typedef enum NotifyVmexitOption {
    NOTIFY_VMEXIT_OPTION_RUN,
    NOTIFY_VMEXIT_OPTION_INTERNAL_ERROR,
    NOTIFY_VMEXIT_OPTION_DISABLE,
    NOTIFY_VMEXIT_OPTION__MAX,
} NotifyVmexitOption;
extern const QEnumLookup NotifyVmexitOption_lookup;
struct StatusInfo {
    
   _Bool 
        running;
    RunState status;
};
void qapi_free_StatusInfo(StatusInfo *obj);
typedef StatusInfo *StatusInfo_autoptr; typedef GList *StatusInfo_listautoptr; typedef GSList *StatusInfo_slistautoptr; typedef GQueue *StatusInfo_queueautoptr;
struct q_obj_SHUTDOWN_arg {
    
   _Bool 
        guest;
    ShutdownCause reason;
};
struct q_obj_RESET_arg {
    
   _Bool 
        guest;
    ShutdownCause reason;
};
struct q_obj_WATCHDOG_arg {
    WatchdogAction action;
};
struct q_obj_watchdog_set_action_arg {
    WatchdogAction action;
};
struct q_obj_set_action_arg {
    
   _Bool 
        has_reboot;
    RebootAction reboot;
    
   _Bool 
        has_shutdown;
    ShutdownAction shutdown;
    
   _Bool 
        has_panic;
    PanicAction panic;
    
   _Bool 
        has_watchdog;
    WatchdogAction watchdog;
};
struct q_obj_GUEST_PANICKED_arg {
    GuestPanicAction action;
    GuestPanicInformation *info;
};
struct q_obj_GUEST_CRASHLOADED_arg {
    GuestPanicAction action;
    GuestPanicInformation *info;
};
struct q_obj_GuestPanicInformation_base {
    GuestPanicInformationType type;
};
struct GuestPanicInformationHyperV {
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t arg5;
};
struct GuestPanicInformationS390 {
    uint32_t core;
    uint64_t psw_mask;
    uint64_t psw_addr;
    S390CrashReason reason;
};
struct GuestPanicInformation {
    GuestPanicInformationType type;
    union {
        GuestPanicInformationHyperV hyper_v;
        GuestPanicInformationS390 s390;
    } u;
};
void qapi_free_GuestPanicInformation(GuestPanicInformation *obj);
typedef GuestPanicInformation *GuestPanicInformation_autoptr; typedef GList *GuestPanicInformation_listautoptr; typedef GSList *GuestPanicInformation_slistautoptr; typedef GQueue *GuestPanicInformation_queueautoptr;
void qapi_free_GuestPanicInformationHyperV(GuestPanicInformationHyperV *obj);
typedef GuestPanicInformationHyperV *GuestPanicInformationHyperV_autoptr; typedef GList *GuestPanicInformationHyperV_listautoptr; typedef GSList *GuestPanicInformationHyperV_slistautoptr; typedef GQueue *GuestPanicInformationHyperV_queueautoptr;
void qapi_free_GuestPanicInformationS390(GuestPanicInformationS390 *obj);
typedef GuestPanicInformationS390 *GuestPanicInformationS390_autoptr; typedef GList *GuestPanicInformationS390_listautoptr; typedef GSList *GuestPanicInformationS390_slistautoptr; typedef GQueue *GuestPanicInformationS390_queueautoptr;
struct q_obj_MEMORY_FAILURE_arg {
    MemoryFailureRecipient recipient;
    MemoryFailureAction action;
    MemoryFailureFlags *flags;
};
struct MemoryFailureFlags {
    
   _Bool 
        action_required;
    
   _Bool 
        recursive;
};
void qapi_free_MemoryFailureFlags(MemoryFailureFlags *obj);
typedef MemoryFailureFlags *MemoryFailureFlags_autoptr; typedef GList *MemoryFailureFlags_listautoptr; typedef GSList *MemoryFailureFlags_slistautoptr; typedef GQueue *MemoryFailureFlags_queueautoptr;
typedef struct QemuLockCnt QemuLockCnt;
struct QemuLockCnt {
    unsigned count;
};
void qemu_lockcnt_init(QemuLockCnt *lockcnt);
void qemu_lockcnt_destroy(QemuLockCnt *lockcnt);
void qemu_lockcnt_inc(QemuLockCnt *lockcnt);
void qemu_lockcnt_dec(QemuLockCnt *lockcnt);
_Bool 
    qemu_lockcnt_dec_and_lock(QemuLockCnt *lockcnt);
_Bool 
    qemu_lockcnt_dec_if_lock(QemuLockCnt *lockcnt);
void qemu_lockcnt_lock(QemuLockCnt *lockcnt);
void qemu_lockcnt_unlock(QemuLockCnt *lockcnt);
void qemu_lockcnt_inc_and_unlock(QemuLockCnt *lockcnt);
unsigned qemu_lockcnt_count(QemuLockCnt *lockcnt);
typedef int (*WriteCoreDumpFunction)(const void *buf, size_t size,
                                     void *opaque);
typedef struct CPUClass CPUClass;
typedef struct CPUWatchpoint CPUWatchpoint;
struct CPUAddressSpace;
struct CPUJumpCache;
struct AccelCPUClass;
struct SysemuCPUOps;
struct CPUClass {
    DeviceClass parent_class;
    ObjectClass *(*class_by_name)(const char *cpu_model);
    void (*parse_features)(const char *typename, char *str, Error **errp);
    
   _Bool 
        (*has_work)(CPUState *cpu);
    int (*mmu_index)(CPUState *cpu, 
                                   _Bool 
                                        ifetch);
    int (*memory_rw_debug)(CPUState *cpu, vaddr addr,
                           uint8_t *buf, int len, 
                                                 _Bool 
                                                      is_write);
    void (*dump_state)(CPUState *cpu, FILE *, int flags);
    void (*query_cpu_fast)(CPUState *cpu, CpuInfoFast *value);
    int64_t (*get_arch_id)(CPUState *cpu);
    void (*set_pc)(CPUState *cpu, vaddr value);
    vaddr (*get_pc)(CPUState *cpu);
    int (*gdb_read_register)(CPUState *cpu, GByteArray *buf, int reg);
    int (*gdb_write_register)(CPUState *cpu, uint8_t *buf, int reg);
    vaddr (*gdb_adjust_breakpoint)(CPUState *cpu, vaddr addr);
    const char *gdb_core_xml_file;
    const gchar * (*gdb_arch_name)(CPUState *cpu);
    void (*disas_set_info)(CPUState *cpu, disassemble_info *info);
    const char *deprecation_note;
    struct AccelCPUClass *accel_cpu;
    const struct SysemuCPUOps *sysemu_ops;
    const TCGCPUOps *tcg_ops;
    void (*init_accel_cpu)(struct AccelCPUClass *accel_cpu, CPUClass *cc);
    int reset_dump_flags;
    int gdb_num_core_regs;
    
   _Bool 
        gdb_stop_before_watchpoint;
};
struct CPUTLBEntryFull {
    hwaddr xlat_section;
    hwaddr phys_addr;
    MemTxAttrs attrs;
    uint8_t prot;
    uint8_t lg_page_size;
    uint8_t tlb_fill_flags;
    uint8_t slow_flags[3];
    union {
        struct {
            uint8_t pte_attrs;
            uint8_t shareability;
            
           _Bool 
                guarded;
        } arm;
    } extra;
};
typedef struct CPUTLBDesc {
    vaddr large_page_addr;
    vaddr large_page_mask;
    int64_t window_begin_ns;
    size_t window_max_entries;
    size_t n_used_entries;
    size_t vindex;
    CPUTLBEntry vtable[8];
    CPUTLBEntryFull vfulltlb[8];
    CPUTLBEntryFull *fulltlb;
} CPUTLBDesc;
typedef struct CPUTLBCommon {
    QemuSpin lock;
    uint16_t dirty;
    size_t full_flush_count;
    size_t part_flush_count;
    size_t elide_flush_count;
} CPUTLBCommon;
typedef struct CPUTLB {
    CPUTLBCommon c;
    CPUTLBDesc d[16];
    CPUTLBDescFast f[16];
} CPUTLB;
typedef union IcountDecr {
    uint32_t u32;
    struct {
        uint16_t low;
        uint16_t high;
    } u16;
} IcountDecr;
typedef struct CPUNegativeOffsetState {
    CPUTLB tlb;
    GArray *plugin_mem_cbs;
    uint64_t plugin_mem_value_low;
    uint64_t plugin_mem_value_high;
    IcountDecr icount_decr;
    
   _Bool 
        can_do_io;
} CPUNegativeOffsetState;
struct KVMState;
struct kvm_run;
typedef union {
    int host_int;
    unsigned long host_ulong;
    void *host_ptr;
    vaddr target_ptr;
} run_on_cpu_data;
typedef void (*run_on_cpu_func)(CPUState *cpu, run_on_cpu_data data);
struct qemu_work_item;
struct CPUState {
    DeviceState parent_obj;
    CPUClass *cc;
    int nr_threads;
    struct QemuThread *thread;
    int thread_id;
    
   _Bool 
        running, has_waiter;
    struct QemuCond *halt_cond;
    
   _Bool 
        thread_kicked;
    
   _Bool 
        created;
    
   _Bool 
        stop;
    
   _Bool 
        stopped;
    
   _Bool 
        start_powered_off;
    
   _Bool 
        unplug;
    
   _Bool 
        crash_occurred;
    
   _Bool 
        exit_request;
    int exclusive_context_count;
    uint32_t cflags_next_tb;
    uint32_t interrupt_request;
    int singlestep_enabled;
    int64_t icount_budget;
    int64_t icount_extra;
    uint64_t random_seed;
    sigjmp_buf jmp_env;
    QemuMutex work_mutex;
    struct { struct qemu_work_item *sqh_first; struct qemu_work_item **sqh_last; } work_list;
    struct CPUAddressSpace *cpu_ases;
    int cpu_ases_count;
    int num_ases;
    AddressSpace *as;
    MemoryRegion *memory;
    struct CPUJumpCache *tb_jmp_cache;
    GArray *gdb_regs;
    int gdb_num_regs;
    int gdb_num_g_regs;
    union { struct CPUState *tqe_next; QTailQLink tqe_circ; } node;
    union { struct CPUBreakpoint *tqh_first; QTailQLink tqh_circ; } breakpoints;
    union { struct CPUWatchpoint *tqh_first; QTailQLink tqh_circ; } watchpoints;
    CPUWatchpoint *watchpoint_hit;
    void *opaque;
    uintptr_t mem_io_pc;
    int kvm_fd;
    struct KVMState *kvm_state;
    struct kvm_run *kvm_run;
    struct kvm_dirty_gfn *kvm_dirty_gfns;
    uint32_t kvm_fetch_index;
    uint64_t dirty_pages;
    int kvm_vcpu_stats_fd;
    
   _Bool 
        vcpu_dirty;
    QemuLockCnt in_ioctl_lock;
    CPUPluginState *plugin_state;
    int cpu_index;
    int cluster_index;
    uint32_t tcg_cflags;
    uint32_t halted;
    int32_t exception_index;
    AccelCPUState *accel;
    
   _Bool 
        throttle_thread_scheduled;
    int64_t throttle_us_per_full;
    
   _Bool 
        ignore_memory_transaction_failures;
    
   _Bool 
        prctl_unalign_sigbus;
    GArray *iommu_notifiers;
    char neg_align[-sizeof(CPUNegativeOffsetState) % 16] __attribute__((aligned(16)));
    CPUNegativeOffsetState neg;
};
_Static_assert(!(
__builtin_offsetof (
CPUState
, 
neg
) 
!= sizeof(CPUState) - sizeof(CPUNegativeOffsetState)), "not expecting: " "offsetof(CPUState, neg) != sizeof(CPUState) - sizeof(CPUNegativeOffsetState)")
                                                                    ;
typedef union CPUTailQ { struct CPUState *tqh_first; QTailQLink tqh_circ; } CPUTailQ;
extern CPUTailQ cpus_queue;
extern __thread CPUState *current_cpu;
extern 
      _Bool 
           mttcg_enabled;
_Bool 
    cpu_paging_enabled(const CPUState *cpu);
_Bool 
    cpu_get_memory_mapping(CPUState *cpu, MemoryMappingList *list,
                            Error **errp);
int cpu_write_elf64_note(WriteCoreDumpFunction f, CPUState *cpu,
                         int cpuid, void *opaque);
int cpu_write_elf64_qemunote(WriteCoreDumpFunction f, CPUState *cpu,
                             void *opaque);
int cpu_write_elf32_note(WriteCoreDumpFunction f, CPUState *cpu,
                         int cpuid, void *opaque);
int cpu_write_elf32_qemunote(WriteCoreDumpFunction f, CPUState *cpu,
                             void *opaque);
GuestPanicInformation *cpu_get_crash_info(CPUState *cpu);
enum CPUDumpFlags {
    CPU_DUMP_CODE = 0x00010000,
    CPU_DUMP_FPU = 0x00020000,
    CPU_DUMP_CCOP = 0x00040000,
    CPU_DUMP_VPU = 0x00080000,
};
void cpu_dump_state(CPUState *cpu, FILE *f, int flags);
hwaddr cpu_get_phys_page_attrs_debug(CPUState *cpu, vaddr addr,
                                     MemTxAttrs *attrs);
hwaddr cpu_get_phys_page_debug(CPUState *cpu, vaddr addr);
int cpu_asidx_from_attrs(CPUState *cpu, MemTxAttrs attrs);
_Bool 
    cpu_virtio_is_big_endian(CPUState *cpu);
void cpu_list_add(CPUState *cpu);
void cpu_list_remove(CPUState *cpu);
void cpu_reset(CPUState *cpu);
ObjectClass *cpu_class_by_name(const char *typename, const char *cpu_model);
char *cpu_model_from_type(const char *typename);
CPUState *cpu_create(const char *typename);
const char *parse_cpu_option(const char *cpu_option);
_Bool 
    qemu_cpu_is_self(CPUState *cpu);
void qemu_cpu_kick(CPUState *cpu);
_Bool 
    cpu_is_stopped(CPUState *cpu);
void do_run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data,
                   QemuMutex *mutex);
void run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data);
void async_run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data);
void async_safe_run_on_cpu(CPUState *cpu, run_on_cpu_func func, run_on_cpu_data data);
CPUState *qemu_get_cpu(int index);
_Bool 
    cpu_exists(int64_t id);
CPUState *cpu_by_arch_id(int64_t id);
void cpu_interrupt(CPUState *cpu, int mask);
void cpu_reset_interrupt(CPUState *cpu, int mask);
void cpu_exit(CPUState *cpu);
void cpu_pause(CPUState *cpu);
void cpu_resume(CPUState *cpu);
void cpu_remove_sync(CPUState *cpu);
void free_queued_cpu_work(CPUState *cpu);
void process_queued_cpu_work(CPUState *cpu);
void cpu_exec_start(CPUState *cpu);
void cpu_exec_end(CPUState *cpu);
void start_exclusive(void);
void end_exclusive(void);
void qemu_init_vcpu(CPUState *cpu);
void cpu_single_step(CPUState *cpu, int enabled);
int cpu_breakpoint_insert(CPUState *cpu, vaddr pc, int flags,
                          CPUBreakpoint **breakpoint);
int cpu_breakpoint_remove(CPUState *cpu, vaddr pc, int flags);
void cpu_breakpoint_remove_by_ref(CPUState *cpu, CPUBreakpoint *breakpoint);
void cpu_breakpoint_remove_all(CPUState *cpu, int mask);
int cpu_watchpoint_insert(CPUState *cpu, vaddr addr, vaddr len,
                          int flags, CPUWatchpoint **watchpoint);
int cpu_watchpoint_remove(CPUState *cpu, vaddr addr,
                          vaddr len, int flags);
void cpu_watchpoint_remove_by_ref(CPUState *cpu, CPUWatchpoint *watchpoint);
void cpu_watchpoint_remove_all(CPUState *cpu, int mask);
AddressSpace *cpu_get_address_space(CPUState *cpu, int asidx);
__attribute__ ((__noreturn__)) void cpu_abort(CPUState *cpu, const char *fmt, ...)
    __attribute__((__format__ (gnu_printf, 2, 3)));
void cpu_class_init_props(DeviceClass *dc);
void cpu_exec_initfn(CPUState *cpu);
_Bool 
    cpu_exec_realizefn(CPUState *cpu, Error **errp);
void cpu_exec_unrealizefn(CPUState *cpu);
void cpu_exec_reset_hold(CPUState *cpu);
const char *target_name(void);
extern const VMStateDescription vmstate_cpu_common;
void cpu_exec_init_all(void);
void cpu_exec_step_atomic(CPUState *cpu);
extern QemuMutex qemu_cpu_list_lock;
void qemu_init_cpu_list(void);
void cpu_list_lock(void);
void cpu_list_unlock(void);
unsigned int cpu_list_generation_id_get(void);
int cpu_get_free_index(void);
void tcg_iommu_init_notifier_list(CPUState *cpu);
void tcg_iommu_free_notifier_list(CPUState *cpu);
enum device_endian {
    DEVICE_NATIVE_ENDIAN,
    DEVICE_BIG_ENDIAN,
    DEVICE_LITTLE_ENDIAN,
};
typedef uint64_t ram_addr_t;
void qemu_ram_remap(ram_addr_t addr, ram_addr_t length);
ram_addr_t qemu_ram_addr_from_host(void *ptr);
ram_addr_t qemu_ram_addr_from_host_nofail(void *ptr);
RAMBlock *qemu_ram_block_by_name(const char *name);
RAMBlock *qemu_ram_block_from_host(void *ptr, 
                                             _Bool 
                                                  round_offset,
                                   ram_addr_t *offset);
ram_addr_t qemu_ram_block_host_offset(RAMBlock *rb, void *host);
void qemu_ram_set_idstr(RAMBlock *block, const char *name, DeviceState *dev);
void qemu_ram_unset_idstr(RAMBlock *block);
const char *qemu_ram_get_idstr(RAMBlock *rb);
void *qemu_ram_get_host_addr(RAMBlock *rb);
ram_addr_t qemu_ram_get_offset(RAMBlock *rb);
ram_addr_t qemu_ram_get_used_length(RAMBlock *rb);
ram_addr_t qemu_ram_get_max_length(RAMBlock *rb);
_Bool 
    qemu_ram_is_shared(RAMBlock *rb);
_Bool 
    qemu_ram_is_noreserve(RAMBlock *rb);
_Bool 
    qemu_ram_is_uf_zeroable(RAMBlock *rb);
void qemu_ram_set_uf_zeroable(RAMBlock *rb);
_Bool 
    qemu_ram_is_migratable(RAMBlock *rb);
void qemu_ram_set_migratable(RAMBlock *rb);
void qemu_ram_unset_migratable(RAMBlock *rb);
_Bool 
    qemu_ram_is_named_file(RAMBlock *rb);
int qemu_ram_get_fd(RAMBlock *rb);
size_t qemu_ram_pagesize(RAMBlock *block);
size_t qemu_ram_pagesize_largest(void);
void cpu_address_space_init(CPUState *cpu, int asidx,
                            const char *prefix, MemoryRegion *mr);
void cpu_address_space_destroy(CPUState *cpu, int asidx);
void cpu_physical_memory_rw(hwaddr addr, void *buf,
                            hwaddr len, 
                                       _Bool 
                                            is_write);
void *cpu_physical_memory_map(hwaddr addr,
                              hwaddr *plen,
                              
                             _Bool 
                                  is_write);
void cpu_physical_memory_unmap(void *buffer, hwaddr len,
                               
                              _Bool 
                                   is_write, hwaddr access_len);
_Bool 
    cpu_physical_memory_is_io(hwaddr phys_addr);
void qemu_flush_coalesced_mmio_buffer(void);
void cpu_flush_icache_range(hwaddr start, hwaddr len);
typedef int (RAMBlockIterFunc)(RAMBlock *rb, void *opaque);
int qemu_ram_foreach_block(RAMBlockIterFunc func, void *opaque);
int ram_block_discard_range(RAMBlock *rb, uint64_t start, size_t length);
int ram_block_discard_guest_memfd_range(RAMBlock *rb, uint64_t start,
                                        size_t length);
int cpu_memory_rw_debug(CPUState *cpu, vaddr addr,
                        void *ptr, size_t len, 
                                              _Bool 
                                                   is_write);
void list_cpus(void);
_Bool 
    cpu_unwind_state_data(CPUState *cpu, uintptr_t host_pc, uint64_t *data);
_Bool 
    cpu_restore_state(CPUState *cpu, uintptr_t host_pc);
__attribute__ ((__noreturn__)) void cpu_loop_exit_noexc(CPUState *cpu);
__attribute__ ((__noreturn__)) void cpu_loop_exit_atomic(CPUState *cpu, uintptr_t pc);
__attribute__ ((__noreturn__)) void cpu_loop_exit(CPUState *cpu);
__attribute__ ((__noreturn__)) void cpu_loop_exit_restore(CPUState *cpu, uintptr_t pc);
int cpu_exec(CPUState *cpu);
typedef int32_t target_long;
typedef uint32_t target_ulong;
_Bool 
    runstate_check(RunState state);
void runstate_set(RunState new_state);
RunState runstate_get(void);
_Bool 
    runstate_is_running(void);
_Bool 
    runstate_needs_reset(void);
void runstate_replay_enable(void);
typedef void VMChangeStateHandler(void *opaque, 
                                               _Bool 
                                                    running, RunState state);
VMChangeStateEntry *qemu_add_vm_change_state_handler(VMChangeStateHandler *cb,
                                                     void *opaque);
VMChangeStateEntry *qemu_add_vm_change_state_handler_prio(
        VMChangeStateHandler *cb, void *opaque, int priority);
VMChangeStateEntry *
qemu_add_vm_change_state_handler_prio_full(VMChangeStateHandler *cb,
                                           VMChangeStateHandler *prepare_cb,
                                           void *opaque, int priority);
VMChangeStateEntry *qdev_add_vm_change_state_handler(DeviceState *dev,
                                                     VMChangeStateHandler *cb,
                                                     void *opaque);
VMChangeStateEntry *qdev_add_vm_change_state_handler_full(
    DeviceState *dev, VMChangeStateHandler *cb,
    VMChangeStateHandler *prepare_cb, void *opaque);
void qemu_del_vm_change_state_handler(VMChangeStateEntry *e);
void vm_state_notify(
                    _Bool 
                         running, RunState state);
void vm_start(void);
int vm_prepare_start(
                    _Bool 
                         step_pending);
void vm_resume(RunState state);
int vm_stop(RunState state);
int vm_stop_force_state(RunState state);
int vm_shutdown(void);
void vm_set_suspended(
                     _Bool 
                          suspended);
_Bool 
    vm_get_suspended(void);
typedef enum WakeupReason {
    QEMU_WAKEUP_REASON_NONE = 0,
    QEMU_WAKEUP_REASON_RTC,
    QEMU_WAKEUP_REASON_PMTIMER,
    QEMU_WAKEUP_REASON_OTHER,
} WakeupReason;
void qemu_system_reset_request(ShutdownCause reason);
void qemu_system_suspend_request(void);
void qemu_register_suspend_notifier(Notifier *notifier);
_Bool 
    qemu_wakeup_suspend_enabled(void);
void qemu_system_wakeup_request(WakeupReason reason, Error **errp);
void qemu_system_wakeup_enable(WakeupReason reason, 
                                                   _Bool 
                                                        enabled);
void qemu_register_wakeup_notifier(Notifier *notifier);
void qemu_register_wakeup_support(void);
void qemu_system_shutdown_request_with_code(ShutdownCause reason,
                                            int exit_code);
void qemu_system_shutdown_request(ShutdownCause reason);
void qemu_system_powerdown_request(void);
void qemu_register_powerdown_notifier(Notifier *notifier);
void qemu_register_shutdown_notifier(Notifier *notifier);
void qemu_system_debug_request(void);
void qemu_system_vmstop_request(RunState reason);
void qemu_system_vmstop_request_prepare(void);
_Bool 
    qemu_vmstop_requested(RunState *r);
ShutdownCause qemu_shutdown_requested_get(void);
ShutdownCause qemu_reset_requested_get(void);
void qemu_system_killed(int signal, pid_t pid);
void qemu_system_reset(ShutdownCause reason);
void qemu_system_guest_panicked(GuestPanicInformation *info);
void qemu_system_guest_crashloaded(GuestPanicInformation *info);
void qemu_system_guest_pvshutdown(void);
_Bool 
    qemu_system_dump_in_progress(void);
typedef void QEMUConfigCB(const char *group, QDict *qdict, void *opaque, Error **errp);
void qemu_load_module_for_opts(const char *group);
QemuOptsList *qemu_find_opts(const char *group);
QemuOptsList *qemu_find_opts_err(const char *group, Error **errp);
QemuOpts *qemu_find_opts_singleton(const char *group);
extern QemuOptsList *vm_config_groups[];
extern QemuOptsList *drive_config_groups[];
void qemu_add_opts(QemuOptsList *list);
void qemu_add_drive_opts(QemuOptsList *list);
int qemu_global_option(const char *str);
int qemu_config_parse(FILE *fp, QemuOptsList **lists, const char *fname,
                      Error **errp);
void qemu_config_do_parse(const char *group, QDict *qdict, void *opaque, Error **errp);
int qemu_read_config_file(const char *filename, QEMUConfigCB *f, Error **errp);
_Bool 
    qemu_config_parse_qdict(QDict *options, QemuOptsList **lists,
                             Error **errp);
typedef uint64_t qemu_plugin_id_t;
extern __attribute__((visibility("default"))) int qemu_plugin_version;
typedef struct qemu_info_t {
    const char *target_name;
    struct {
        int min;
        int cur;
    } version;
    
   _Bool 
        system_emulation;
    union {
        struct {
            int smp_vcpus;
            int max_vcpus;
        } system;
    };
} qemu_info_t;
__attribute__((visibility("default"))) int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv);
typedef void (*qemu_plugin_simple_cb_t)(qemu_plugin_id_t id);
typedef void (*qemu_plugin_udata_cb_t)(qemu_plugin_id_t id, void *userdata);
typedef void (*qemu_plugin_vcpu_simple_cb_t)(qemu_plugin_id_t id,
                                             unsigned int vcpu_index);
typedef void (*qemu_plugin_vcpu_udata_cb_t)(unsigned int vcpu_index,
                                            void *userdata);
void qemu_plugin_uninstall(qemu_plugin_id_t id, qemu_plugin_simple_cb_t cb);
void qemu_plugin_reset(qemu_plugin_id_t id, qemu_plugin_simple_cb_t cb);
void qemu_plugin_register_vcpu_init_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);
void qemu_plugin_register_vcpu_exit_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);
void qemu_plugin_register_vcpu_idle_cb(qemu_plugin_id_t id,
                                       qemu_plugin_vcpu_simple_cb_t cb);
void qemu_plugin_register_vcpu_resume_cb(qemu_plugin_id_t id,
                                         qemu_plugin_vcpu_simple_cb_t cb);
struct qemu_plugin_tb;
struct qemu_plugin_insn;
struct qemu_plugin_scoreboard;
typedef struct {
    struct qemu_plugin_scoreboard *score;
    size_t offset;
} qemu_plugin_u64;
enum qemu_plugin_cb_flags {
    QEMU_PLUGIN_CB_NO_REGS,
    QEMU_PLUGIN_CB_R_REGS,
    QEMU_PLUGIN_CB_RW_REGS,
};
enum qemu_plugin_mem_rw {
    QEMU_PLUGIN_MEM_R = 1,
    QEMU_PLUGIN_MEM_W,
    QEMU_PLUGIN_MEM_RW,
};
enum qemu_plugin_mem_value_type {
    QEMU_PLUGIN_MEM_VALUE_U8,
    QEMU_PLUGIN_MEM_VALUE_U16,
    QEMU_PLUGIN_MEM_VALUE_U32,
    QEMU_PLUGIN_MEM_VALUE_U64,
    QEMU_PLUGIN_MEM_VALUE_U128,
};
typedef struct {
    enum qemu_plugin_mem_value_type type;
    union {
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
        struct {
            uint64_t low;
            uint64_t high;
        } u128;
    } data;
} qemu_plugin_mem_value;
enum qemu_plugin_cond {
    QEMU_PLUGIN_COND_NEVER,
    QEMU_PLUGIN_COND_ALWAYS,
    QEMU_PLUGIN_COND_EQ,
    QEMU_PLUGIN_COND_NE,
    QEMU_PLUGIN_COND_LT,
    QEMU_PLUGIN_COND_LE,
    QEMU_PLUGIN_COND_GT,
    QEMU_PLUGIN_COND_GE,
};
typedef void (*qemu_plugin_vcpu_tb_trans_cb_t)(qemu_plugin_id_t id,
                                               struct qemu_plugin_tb *tb);
void qemu_plugin_register_vcpu_tb_trans_cb(qemu_plugin_id_t id,
                                           qemu_plugin_vcpu_tb_trans_cb_t cb);
void qemu_plugin_register_vcpu_tb_exec_cb(struct qemu_plugin_tb *tb,
                                          qemu_plugin_vcpu_udata_cb_t cb,
                                          enum qemu_plugin_cb_flags flags,
                                          void *userdata);
void qemu_plugin_register_vcpu_tb_exec_cond_cb(struct qemu_plugin_tb *tb,
                                               qemu_plugin_vcpu_udata_cb_t cb,
                                               enum qemu_plugin_cb_flags flags,
                                               enum qemu_plugin_cond cond,
                                               qemu_plugin_u64 entry,
                                               uint64_t imm,
                                               void *userdata);
enum qemu_plugin_op {
    QEMU_PLUGIN_INLINE_ADD_U64,
    QEMU_PLUGIN_INLINE_STORE_U64,
};
void qemu_plugin_register_vcpu_tb_exec_inline_per_vcpu(
    struct qemu_plugin_tb *tb,
    enum qemu_plugin_op op,
    qemu_plugin_u64 entry,
    uint64_t imm);
void qemu_plugin_register_vcpu_insn_exec_cb(struct qemu_plugin_insn *insn,
                                            qemu_plugin_vcpu_udata_cb_t cb,
                                            enum qemu_plugin_cb_flags flags,
                                            void *userdata);
void qemu_plugin_register_vcpu_insn_exec_cond_cb(
    struct qemu_plugin_insn *insn,
    qemu_plugin_vcpu_udata_cb_t cb,
    enum qemu_plugin_cb_flags flags,
    enum qemu_plugin_cond cond,
    qemu_plugin_u64 entry,
    uint64_t imm,
    void *userdata);
void qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(
    struct qemu_plugin_insn *insn,
    enum qemu_plugin_op op,
    qemu_plugin_u64 entry,
    uint64_t imm);
size_t qemu_plugin_tb_n_insns(const struct qemu_plugin_tb *tb);
size_t qemu_plugin_tb_size(const struct qemu_plugin_tb *tb);
uint64_t qemu_plugin_tb_vaddr(const struct qemu_plugin_tb *tb);
struct qemu_plugin_insn *
qemu_plugin_tb_get_insn(const struct qemu_plugin_tb *tb, size_t idx);
struct qemu_plugin_insn *
qemu_plugin_tb_get_insn_by_vaddr(const struct qemu_plugin_tb *tb, uint64_t vaddr);
size_t qemu_plugin_insn_data(const struct qemu_plugin_insn *insn,
                             void *dest, size_t len);
size_t qemu_plugin_insn_size(const struct qemu_plugin_insn *insn);
uint64_t qemu_plugin_insn_vaddr(const struct qemu_plugin_insn *insn);
void *qemu_plugin_insn_haddr(const struct qemu_plugin_insn *insn);
typedef uint32_t qemu_plugin_meminfo_t;
struct qemu_plugin_hwaddr;
unsigned int qemu_plugin_mem_size_shift(qemu_plugin_meminfo_t info);
_Bool 
    qemu_plugin_mem_is_sign_extended(qemu_plugin_meminfo_t info);
_Bool 
    qemu_plugin_mem_is_big_endian(qemu_plugin_meminfo_t info);
_Bool 
    qemu_plugin_mem_is_store(qemu_plugin_meminfo_t info);
qemu_plugin_mem_value qemu_plugin_mem_get_value(qemu_plugin_meminfo_t info);
struct qemu_plugin_hwaddr *qemu_plugin_get_hwaddr(qemu_plugin_meminfo_t info,
                                                  uint64_t vaddr);
_Bool 
    qemu_plugin_hwaddr_is_io(const struct qemu_plugin_hwaddr *haddr);
uint64_t qemu_plugin_hwaddr_phys_addr(const struct qemu_plugin_hwaddr *haddr);
const char *qemu_plugin_hwaddr_device_name(const struct qemu_plugin_hwaddr *h);
typedef void (*qemu_plugin_vcpu_mem_cb_t) (unsigned int vcpu_index,
                                           qemu_plugin_meminfo_t info,
                                           uint64_t vaddr,
                                           void *userdata);
void qemu_plugin_register_vcpu_mem_cb(struct qemu_plugin_insn *insn,
                                      qemu_plugin_vcpu_mem_cb_t cb,
                                      enum qemu_plugin_cb_flags flags,
                                      enum qemu_plugin_mem_rw rw,
                                      void *userdata);
void qemu_plugin_register_vcpu_mem_inline_per_vcpu(
    struct qemu_plugin_insn *insn,
    enum qemu_plugin_mem_rw rw,
    enum qemu_plugin_op op,
    qemu_plugin_u64 entry,
    uint64_t imm);
const void *qemu_plugin_request_time_control(void);
void qemu_plugin_update_ns(const void *handle, int64_t time);
typedef void
(*qemu_plugin_vcpu_syscall_cb_t)(qemu_plugin_id_t id, unsigned int vcpu_index,
                                 int64_t num, uint64_t a1, uint64_t a2,
                                 uint64_t a3, uint64_t a4, uint64_t a5,
                                 uint64_t a6, uint64_t a7, uint64_t a8);
void qemu_plugin_register_vcpu_syscall_cb(qemu_plugin_id_t id,
                                          qemu_plugin_vcpu_syscall_cb_t cb);
typedef void
(*qemu_plugin_vcpu_syscall_ret_cb_t)(qemu_plugin_id_t id, unsigned int vcpu_idx,
                                     int64_t num, int64_t ret);
void
qemu_plugin_register_vcpu_syscall_ret_cb(qemu_plugin_id_t id,
                                         qemu_plugin_vcpu_syscall_ret_cb_t cb);
char *qemu_plugin_insn_disas(const struct qemu_plugin_insn *insn);
const char *qemu_plugin_insn_symbol(const struct qemu_plugin_insn *insn);
void qemu_plugin_vcpu_for_each(qemu_plugin_id_t id,
                               qemu_plugin_vcpu_simple_cb_t cb);
void qemu_plugin_register_flush_cb(qemu_plugin_id_t id,
                                   qemu_plugin_simple_cb_t cb);
void qemu_plugin_register_atexit_cb(qemu_plugin_id_t id,
                                    qemu_plugin_udata_cb_t cb, void *userdata);
int qemu_plugin_num_vcpus(void);
void qemu_plugin_outs(const char *string);
_Bool 
    qemu_plugin_bool_parse(const char *name, const char *val, 
                                                              _Bool 
                                                                   *ret);
const char *qemu_plugin_path_to_binary(void);
uint64_t qemu_plugin_start_code(void);
uint64_t qemu_plugin_end_code(void);
uint64_t qemu_plugin_entry_code(void);
struct qemu_plugin_register;
typedef struct {
    struct qemu_plugin_register *handle;
    const char *name;
    const char *feature;
} qemu_plugin_reg_descriptor;
GArray *qemu_plugin_get_registers(void);
_Bool 
    qemu_plugin_read_memory_vaddr(uint64_t addr,
                                   GByteArray *data, size_t len);
int qemu_plugin_read_register(struct qemu_plugin_register *handle,
                              GByteArray *buf);
struct qemu_plugin_scoreboard *qemu_plugin_scoreboard_new(size_t element_size);
void qemu_plugin_scoreboard_free(struct qemu_plugin_scoreboard *score);
void *qemu_plugin_scoreboard_find(struct qemu_plugin_scoreboard *score,
                                  unsigned int vcpu_index);
void qemu_plugin_u64_add(qemu_plugin_u64 entry, unsigned int vcpu_index,
                         uint64_t added);
uint64_t qemu_plugin_u64_get(qemu_plugin_u64 entry, unsigned int vcpu_index);
void qemu_plugin_u64_set(qemu_plugin_u64 entry, unsigned int vcpu_index,
                         uint64_t val);
uint64_t qemu_plugin_u64_sum(qemu_plugin_u64 entry);
typedef struct Location {
    enum { LOC_NONE, LOC_CMDLINE, LOC_FILE } kind;
    int num;
    const void *ptr;
    struct Location *prev;
} Location;
Location *loc_push_restore(Location *loc);
Location *loc_push_none(Location *loc);
Location *loc_pop(Location *loc);
Location *loc_save(Location *loc);
void loc_restore(Location *loc);
void loc_set_none(void);
void loc_set_cmdline(char **argv, int idx, int cnt);
void loc_set_file(const char *fname, int lno);
int error_vprintf(const char *fmt, va_list ap) __attribute__((__format__ (gnu_printf, 1, 0)));
int error_printf(const char *fmt, ...) __attribute__((__format__ (gnu_printf, 1, 2)));
void error_vreport(const char *fmt, va_list ap) __attribute__((__format__ (gnu_printf, 1, 0)));
void warn_vreport(const char *fmt, va_list ap) __attribute__((__format__ (gnu_printf, 1, 0)));
void info_vreport(const char *fmt, va_list ap) __attribute__((__format__ (gnu_printf, 1, 0)));
void error_report(const char *fmt, ...) __attribute__((__format__ (gnu_printf, 1, 2)));
void warn_report(const char *fmt, ...) __attribute__((__format__ (gnu_printf, 1, 2)));
void info_report(const char *fmt, ...) __attribute__((__format__ (gnu_printf, 1, 2)));
_Bool 
    error_report_once_cond(
                           _Bool 
                                *printed, const char *fmt, ...)
    __attribute__((__format__ (gnu_printf, 2, 3)));
_Bool 
    warn_report_once_cond(
                          _Bool 
                               *printed, const char *fmt, ...)
    __attribute__((__format__ (gnu_printf, 2, 3)));
void error_init(const char *argv0);
extern 
      _Bool 
           message_with_timestamp;
extern 
      _Bool 
           error_with_guestname;
extern const char *error_guest_name;
const char *get_opt_value(const char *p, char **value);
_Bool 
    parse_option_size(const char *name, const char *value,
                       uint64_t *ret, Error **errp);
_Bool 
    has_help_option(const char *param);
enum QemuOptType {
    QEMU_OPT_STRING = 0,
    QEMU_OPT_BOOL,
    QEMU_OPT_NUMBER,
    QEMU_OPT_SIZE,
};
typedef struct QemuOpt QemuOpt;
typedef struct QemuOptDesc {
    const char *name;
    enum QemuOptType type;
    const char *help;
    const char *def_value_str;
} QemuOptDesc;
struct QemuOptsList {
    const char *name;
    const char *implied_opt_name;
    
   _Bool 
        merge_lists;
    union { struct QemuOpts *tqh_first; QTailQLink tqh_circ; } head;
    QemuOptDesc desc[];
};
const char *qemu_opt_get(QemuOpts *opts, const char *name);
char *qemu_opt_get_del(QemuOpts *opts, const char *name);
_Bool 
    qemu_opt_has_help_opt(QemuOpts *opts);
QemuOpt *qemu_opt_find(QemuOpts *opts, const char *name);
_Bool 
    qemu_opt_get_bool(QemuOpts *opts, const char *name, 
                                                        _Bool 
                                                             defval);
uint64_t qemu_opt_get_number(QemuOpts *opts, const char *name, uint64_t defval);
uint64_t qemu_opt_get_size(QemuOpts *opts, const char *name, uint64_t defval);
_Bool 
    qemu_opt_get_bool_del(QemuOpts *opts, const char *name, 
                                                            _Bool 
                                                                 defval);
uint64_t qemu_opt_get_number_del(QemuOpts *opts, const char *name,
                                 uint64_t defval);
uint64_t qemu_opt_get_size_del(QemuOpts *opts, const char *name,
                               uint64_t defval);
int qemu_opt_unset(QemuOpts *opts, const char *name);
_Bool 
    qemu_opt_set(QemuOpts *opts, const char *name, const char *value,
                  Error **errp);
_Bool 
    qemu_opt_set_bool(QemuOpts *opts, const char *name, 
                                                        _Bool 
                                                             val,
                       Error **errp);
_Bool 
    qemu_opt_set_number(QemuOpts *opts, const char *name, int64_t val,
                         Error **errp);
typedef int (*qemu_opt_loopfunc)(void *opaque,
                                 const char *name, const char *value,
                                 Error **errp);
int qemu_opt_foreach(QemuOpts *opts, qemu_opt_loopfunc func, void *opaque,
                     Error **errp);
typedef struct {
    QemuOpts *opts;
    QemuOpt *opt;
    const char *name;
} QemuOptsIter;
void qemu_opt_iter_init(QemuOptsIter *iter, QemuOpts *opts, const char *name);
const char *qemu_opt_iter_next(QemuOptsIter *iter);
QemuOpts *qemu_opts_find(QemuOptsList *list, const char *id);
QemuOpts *qemu_opts_create(QemuOptsList *list, const char *id,
                           int fail_if_exists, Error **errp);
void qemu_opts_reset(QemuOptsList *list);
void qemu_opts_loc_restore(QemuOpts *opts);
const char *qemu_opts_id(QemuOpts *opts);
void qemu_opts_set_id(QemuOpts *opts, char *id);
void qemu_opts_del(QemuOpts *opts);
_Bool 
    qemu_opts_validate(QemuOpts *opts, const QemuOptDesc *desc, Error **errp);
_Bool 
    qemu_opts_do_parse(QemuOpts *opts, const char *params,
                        const char *firstname, Error **errp);
QemuOpts *qemu_opts_parse_noisily(QemuOptsList *list, const char *params,
                                  
                                 _Bool 
                                      permit_abbrev);
QemuOpts *qemu_opts_parse(QemuOptsList *list, const char *params,
                          
                         _Bool 
                              permit_abbrev, Error **errp);
QemuOpts *qemu_opts_from_qdict(QemuOptsList *list, const QDict *qdict,
                               Error **errp);
QDict *qemu_opts_to_qdict_filtered(QemuOpts *opts, QDict *qdict,
                                   QemuOptsList *list, 
                                                      _Bool 
                                                           del);
QDict *qemu_opts_to_qdict(QemuOpts *opts, QDict *qdict);
_Bool 
    qemu_opts_absorb_qdict(QemuOpts *opts, QDict *qdict, Error **errp);
typedef int (*qemu_opts_loopfunc)(void *opaque, QemuOpts *opts, Error **errp);
int qemu_opts_foreach(QemuOptsList *list, qemu_opts_loopfunc func,
                      void *opaque, Error **errp);
void qemu_opts_print(QemuOpts *opts, const char *sep);
void qemu_opts_print_help(QemuOptsList *list, 
                                             _Bool 
                                                  print_caption);
void qemu_opts_free(QemuOptsList *list);
QemuOptsList *qemu_opts_append(QemuOptsList *dst, QemuOptsList *list);
typedef QemuOpts *QemuOpts_autoptr; typedef GList *QemuOpts_listautoptr; typedef GSList *QemuOpts_slistautoptr; typedef GQueue *QemuOpts_queueautoptr;
enum qemu_plugin_event {
    QEMU_PLUGIN_EV_VCPU_INIT,
    QEMU_PLUGIN_EV_VCPU_EXIT,
    QEMU_PLUGIN_EV_VCPU_TB_TRANS,
    QEMU_PLUGIN_EV_VCPU_IDLE,
    QEMU_PLUGIN_EV_VCPU_RESUME,
    QEMU_PLUGIN_EV_VCPU_SYSCALL,
    QEMU_PLUGIN_EV_VCPU_SYSCALL_RET,
    QEMU_PLUGIN_EV_FLUSH,
    QEMU_PLUGIN_EV_ATEXIT,
    QEMU_PLUGIN_EV_MAX,
};
typedef enum MemOp {
    MO_8 = 0,
    MO_16 = 1,
    MO_32 = 2,
    MO_64 = 3,
    MO_128 = 4,
    MO_256 = 5,
    MO_512 = 6,
    MO_1024 = 7,
    MO_SIZE = 0x07,
    MO_SIGN = 0x08,
    MO_BSWAP = 0x10,
    MO_LE = 0,
    MO_BE = MO_BSWAP,
    MO_TE = MO_BE,
    MO_ASHIFT = 5,
    MO_AMASK = 0x7 << MO_ASHIFT,
    MO_UNALN = 0,
    MO_ALIGN_2 = 1 << MO_ASHIFT,
    MO_ALIGN_4 = 2 << MO_ASHIFT,
    MO_ALIGN_8 = 3 << MO_ASHIFT,
    MO_ALIGN_16 = 4 << MO_ASHIFT,
    MO_ALIGN_32 = 5 << MO_ASHIFT,
    MO_ALIGN_64 = 6 << MO_ASHIFT,
    MO_ALIGN = MO_AMASK,
    MO_ATOM_SHIFT = 8,
    MO_ATOM_IFALIGN = 0 << MO_ATOM_SHIFT,
    MO_ATOM_IFALIGN_PAIR = 1 << MO_ATOM_SHIFT,
    MO_ATOM_WITHIN16 = 2 << MO_ATOM_SHIFT,
    MO_ATOM_WITHIN16_PAIR = 3 << MO_ATOM_SHIFT,
    MO_ATOM_SUBALIGN = 4 << MO_ATOM_SHIFT,
    MO_ATOM_NONE = 5 << MO_ATOM_SHIFT,
    MO_ATOM_MASK = 7 << MO_ATOM_SHIFT,
    MO_UB = MO_8,
    MO_UW = MO_16,
    MO_UL = MO_32,
    MO_UQ = MO_64,
    MO_UO = MO_128,
    MO_SB = MO_SIGN | MO_8,
    MO_SW = MO_SIGN | MO_16,
    MO_SL = MO_SIGN | MO_32,
    MO_SQ = MO_SIGN | MO_64,
    MO_SO = MO_SIGN | MO_128,
    MO_LEUW = MO_LE | MO_UW,
    MO_LEUL = MO_LE | MO_UL,
    MO_LEUQ = MO_LE | MO_UQ,
    MO_LESW = MO_LE | MO_SW,
    MO_LESL = MO_LE | MO_SL,
    MO_LESQ = MO_LE | MO_SQ,
    MO_BEUW = MO_BE | MO_UW,
    MO_BEUL = MO_BE | MO_UL,
    MO_BEUQ = MO_BE | MO_UQ,
    MO_BESW = MO_BE | MO_SW,
    MO_BESL = MO_BE | MO_SL,
    MO_BESQ = MO_BE | MO_SQ,
    MO_TEUW = MO_TE | MO_UW,
    MO_TEUL = MO_TE | MO_UL,
    MO_TEUQ = MO_TE | MO_UQ,
    MO_TEUO = MO_TE | MO_UO,
    MO_TESW = MO_TE | MO_SW,
    MO_TESL = MO_TE | MO_SL,
    MO_TESQ = MO_TE | MO_SQ,
    MO_SSIZE = MO_SIZE | MO_SIGN,
} MemOp;
typedef uint32_t MemOpIdx;
struct qemu_plugin_desc;
typedef union { struct qemu_plugin_desc *tqh_first; QTailQLink tqh_circ; } QemuPluginList;
extern QemuOptsList qemu_plugin_opts;
void qemu_plugin_opt_parse(const char *optstr, QemuPluginList *head);
int qemu_plugin_load_list(QemuPluginList *head, Error **errp);
union qemu_plugin_cb_sig {
    qemu_plugin_simple_cb_t simple;
    qemu_plugin_udata_cb_t udata;
    qemu_plugin_vcpu_simple_cb_t vcpu_simple;
    qemu_plugin_vcpu_udata_cb_t vcpu_udata;
    qemu_plugin_vcpu_tb_trans_cb_t vcpu_tb_trans;
    qemu_plugin_vcpu_mem_cb_t vcpu_mem;
    qemu_plugin_vcpu_syscall_cb_t vcpu_syscall;
    qemu_plugin_vcpu_syscall_ret_cb_t vcpu_syscall_ret;
    void *generic;
};
enum plugin_dyn_cb_type {
    PLUGIN_CB_REGULAR,
    PLUGIN_CB_COND,
    PLUGIN_CB_MEM_REGULAR,
    PLUGIN_CB_INLINE_ADD_U64,
    PLUGIN_CB_INLINE_STORE_U64,
};
struct qemu_plugin_regular_cb {
    union qemu_plugin_cb_sig f;
    TCGHelperInfo *info;
    void *userp;
    enum qemu_plugin_mem_rw rw;
};
struct qemu_plugin_inline_cb {
    qemu_plugin_u64 entry;
    uint64_t imm;
    enum qemu_plugin_mem_rw rw;
};
struct qemu_plugin_conditional_cb {
    union qemu_plugin_cb_sig f;
    TCGHelperInfo *info;
    void *userp;
    qemu_plugin_u64 entry;
    enum qemu_plugin_cond cond;
    uint64_t imm;
};
struct qemu_plugin_dyn_cb {
    enum plugin_dyn_cb_type type;
    union {
        struct qemu_plugin_regular_cb regular;
        struct qemu_plugin_conditional_cb cond;
        struct qemu_plugin_inline_cb inline_insn;
    };
};
struct qemu_plugin_insn {
    uint64_t vaddr;
    GArray *insn_cbs;
    GArray *mem_cbs;
    uint8_t len;
    
   _Bool 
        calls_helpers;
    
   _Bool 
        mem_helper;
};
struct qemu_plugin_scoreboard {
    GArray *data;
    struct { struct qemu_plugin_scoreboard *le_next; struct qemu_plugin_scoreboard **le_prev; } entry;
};
struct qemu_plugin_tb {
    GPtrArray *insns;
    size_t n;
    
   _Bool 
        mem_helper;
    GArray *cbs;
};
struct CPUPluginState {
    unsigned long event_mask[(((QEMU_PLUGIN_EV_MAX) + (8 * sizeof(long)) - 1) / (8 * sizeof(long)))];
};
CPUPluginState *qemu_plugin_create_vcpu_state(void);
void qemu_plugin_vcpu_init_hook(CPUState *cpu);
void qemu_plugin_vcpu_exit_hook(CPUState *cpu);
void qemu_plugin_tb_trans_cb(CPUState *cpu, struct qemu_plugin_tb *tb);
void qemu_plugin_vcpu_idle_cb(CPUState *cpu);
void qemu_plugin_vcpu_resume_cb(CPUState *cpu);
void
qemu_plugin_vcpu_syscall(CPUState *cpu, int64_t num, uint64_t a1,
                         uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7, uint64_t a8);
void qemu_plugin_vcpu_syscall_ret(CPUState *cpu, int64_t num, int64_t ret);
void qemu_plugin_vcpu_mem_cb(CPUState *cpu, uint64_t vaddr,
                             uint64_t value_low,
                             uint64_t value_high,
                             MemOpIdx oi, enum qemu_plugin_mem_rw rw);
void qemu_plugin_flush_cb(void);
void qemu_plugin_atexit_cb(void);
void qemu_plugin_add_dyn_cb_arr(GArray *arr);
void qemu_plugin_user_exit(void);
void qemu_plugin_user_prefork_lock(void);
void qemu_plugin_user_postfork(
                              _Bool 
                                   is_child);
typedef struct Coroutine Coroutine;
typedef struct CoMutex CoMutex;
typedef void CoroutineEntry(void *opaque);
Coroutine *qemu_coroutine_create(CoroutineEntry *entry, void *opaque);
void qemu_coroutine_enter(Coroutine *coroutine);
void qemu_coroutine_enter_if_inactive(Coroutine *co);
void qemu_aio_coroutine_enter(AioContext *ctx, Coroutine *co);
void qemu_coroutine_yield(void);
AioContext *qemu_coroutine_get_aio_context(Coroutine *co);
Coroutine *qemu_coroutine_self(void);
_Bool 
    qemu_in_coroutine(void);
_Bool 
    qemu_coroutine_entered(Coroutine *co);
void qemu_co_mutex_init(CoMutex *mutex);
void qemu_co_mutex_lock(CoMutex *mutex);
void qemu_co_mutex_unlock(CoMutex *mutex);
typedef void QemuLockUnlockFunc(void *);
typedef struct QemuLockable {
    void *object;
    QemuLockUnlockFunc *lock;
    QemuLockUnlockFunc *unlock;
} QemuLockable;
typedef QemuLockable *QemuLockable_autoptr; typedef GList *QemuLockable_listautoptr; typedef GSList *QemuLockable_slistautoptr; typedef GQueue *QemuLockable_queueautoptr;
typedef struct QemuSeqLock QemuSeqLock;
struct QemuSeqLock {
    unsigned sequence;
};
struct qdist_entry {
    double x;
    unsigned long count;
};
struct qdist {
    struct qdist_entry *entries;
    size_t n;
    size_t size;
};
void qdist_init(struct qdist *dist);
void qdist_destroy(struct qdist *dist);
void qdist_add(struct qdist *dist, double x, long count);
void qdist_inc(struct qdist *dist, double x);
double qdist_xmin(const struct qdist *dist);
double qdist_xmax(const struct qdist *dist);
double qdist_avg(const struct qdist *dist);
unsigned long qdist_sample_count(const struct qdist *dist);
size_t qdist_unique_entries(const struct qdist *dist);
char *qdist_pr_plain(const struct qdist *dist, size_t n_groups);
char *qdist_pr(const struct qdist *dist, size_t n_groups, uint32_t opt);
void qdist_bin__internal(struct qdist *to, const struct qdist *from, size_t n);
typedef 
       _Bool 
            (*qht_cmp_func_t)(const void *a, const void *b);
struct qht {
    struct qht_map *map;
    qht_cmp_func_t cmp;
    QemuMutex lock;
    unsigned int mode;
};
struct qht_stats {
    size_t head_buckets;
    size_t used_head_buckets;
    size_t entries;
    struct qdist chain;
    struct qdist occupancy;
};
typedef 
       _Bool 
            (*qht_lookup_func_t)(const void *obj, const void *userp);
typedef void (*qht_iter_func_t)(void *p, uint32_t h, void *up);
typedef 
       _Bool 
            (*qht_iter_bool_func_t)(void *p, uint32_t h, void *up);
void qht_init(struct qht *ht, qht_cmp_func_t cmp, size_t n_elems,
              unsigned int mode);
void qht_destroy(struct qht *ht);
_Bool 
    qht_insert(struct qht *ht, void *p, uint32_t hash, void **existing);
void *qht_lookup_custom(const struct qht *ht, const void *userp, uint32_t hash,
                        qht_lookup_func_t func);
void *qht_lookup(const struct qht *ht, const void *userp, uint32_t hash);
_Bool 
    qht_remove(struct qht *ht, const void *p, uint32_t hash);
void qht_reset(struct qht *ht);
_Bool 
    qht_reset_size(struct qht *ht, size_t n_elems);
_Bool 
    qht_resize(struct qht *ht, size_t n_elems);
void qht_iter(struct qht *ht, qht_iter_func_t func, void *userp);
void qht_iter_remove(struct qht *ht, qht_iter_bool_func_t func, void *userp);
void qht_statistics_init(const struct qht *ht, struct qht_stats *stats);
void qht_statistics_destroy(struct qht_stats *stats);
struct qemu_plugin_state {
    union { struct qemu_plugin_ctx *tqh_first; QTailQLink tqh_circ; } ctxs;
    struct { struct qemu_plugin_cb *lh_first; } cb_lists[QEMU_PLUGIN_EV_MAX];
    GHashTable *id_ht;
    GHashTable *cpu_ht;
    struct { struct qemu_plugin_scoreboard *lh_first; } scoreboards;
    size_t scoreboard_alloc_size;
    unsigned long mask[(((QEMU_PLUGIN_EV_MAX) + (8 * sizeof(long)) - 1) / (8 * sizeof(long)))];
    QemuRecMutex lock;
    struct qht dyn_cb_arr_ht;
    int num_vcpus;
};
struct qemu_plugin_ctx {
    GModule *handle;
    qemu_plugin_id_t id;
    struct qemu_plugin_cb *callbacks[QEMU_PLUGIN_EV_MAX];
    union { struct qemu_plugin_ctx *tqe_next; QTailQLink tqe_circ; } entry;
    struct qemu_plugin_desc *desc;
    
   _Bool 
        installing;
    
   _Bool 
        uninstalling;
    
   _Bool 
        resetting;
};
struct qemu_plugin_ctx *plugin_id_to_ctx_locked(qemu_plugin_id_t id);
void plugin_register_inline_op_on_entry(GArray **arr,
                                        enum qemu_plugin_mem_rw rw,
                                        enum qemu_plugin_op op,
                                        qemu_plugin_u64 entry,
                                        uint64_t imm);
void plugin_reset_uninstall(qemu_plugin_id_t id,
                            qemu_plugin_simple_cb_t cb,
                            
                           _Bool 
                                reset);
void plugin_register_cb(qemu_plugin_id_t id, enum qemu_plugin_event ev,
                        void *func);
void plugin_unregister_cb__locked(struct qemu_plugin_ctx *ctx,
                                  enum qemu_plugin_event ev);
void
plugin_register_cb_udata(qemu_plugin_id_t id, enum qemu_plugin_event ev,
                         void *func, void *udata);
void
plugin_register_dyn_cb__udata(GArray **arr,
                              qemu_plugin_vcpu_udata_cb_t cb,
                              enum qemu_plugin_cb_flags flags, void *udata);
void
plugin_register_dyn_cond_cb__udata(GArray **arr,
                                   qemu_plugin_vcpu_udata_cb_t cb,
                                   enum qemu_plugin_cb_flags flags,
                                   enum qemu_plugin_cond cond,
                                   qemu_plugin_u64 entry,
                                   uint64_t imm,
                                   void *udata);
void plugin_register_vcpu_mem_cb(GArray **arr,
                                 void *cb,
                                 enum qemu_plugin_cb_flags flags,
                                 enum qemu_plugin_mem_rw rw,
                                 void *udata);
void exec_inline_op(enum plugin_dyn_cb_type type,
                    struct qemu_plugin_inline_cb *cb,
                    int cpu_index);
int plugin_num_vcpus(void);
struct qemu_plugin_scoreboard *plugin_scoreboard_new(size_t element_size);
void plugin_scoreboard_free(struct qemu_plugin_scoreboard *score);
       
typedef uint16_t float16;
typedef uint32_t float32;
typedef uint64_t float64;
typedef struct {
    uint64_t low;
    uint16_t high;
} floatx80;
typedef struct {
    uint64_t low, high;
} float128;
typedef uint16_t bfloat16;
typedef enum __attribute__((__packed__)) {
    float_round_nearest_even = 0,
    float_round_down = 1,
    float_round_up = 2,
    float_round_to_zero = 3,
    float_round_ties_away = 4,
    float_round_to_odd = 5,
    float_round_to_odd_inf = 6,
    float_round_nearest_even_max = 7,
} FloatRoundMode;
enum {
    float_flag_invalid = 0x0001,
    float_flag_divbyzero = 0x0002,
    float_flag_overflow = 0x0004,
    float_flag_underflow = 0x0008,
    float_flag_inexact = 0x0010,
    float_flag_input_denormal = 0x0020,
    float_flag_output_denormal = 0x0040,
    float_flag_invalid_isi = 0x0080,
    float_flag_invalid_imz = 0x0100,
    float_flag_invalid_idi = 0x0200,
    float_flag_invalid_zdz = 0x0400,
    float_flag_invalid_sqrt = 0x0800,
    float_flag_invalid_cvti = 0x1000,
    float_flag_invalid_snan = 0x2000,
};
typedef enum __attribute__((__packed__)) {
    floatx80_precision_x,
    floatx80_precision_d,
    floatx80_precision_s,
} FloatX80RoundPrec;
typedef enum __attribute__((__packed__)) {
    float_2nan_prop_none = 0,
    float_2nan_prop_s_ab,
    float_2nan_prop_s_ba,
    float_2nan_prop_ab,
    float_2nan_prop_ba,
    float_2nan_prop_x87,
} Float2NaNPropRule;
enum { R_3NAN_1ST_SHIFT = (0)}; enum { R_3NAN_1ST_LENGTH = (2)}; enum { R_3NAN_1ST_MASK = (((~0ULL) >> (64 - (2))) << (0))};
enum { R_3NAN_2ND_SHIFT = (2)}; enum { R_3NAN_2ND_LENGTH = (2)}; enum { R_3NAN_2ND_MASK = (((~0ULL) >> (64 - (2))) << (2))};
enum { R_3NAN_3RD_SHIFT = (4)}; enum { R_3NAN_3RD_LENGTH = (2)}; enum { R_3NAN_3RD_MASK = (((~0ULL) >> (64 - (2))) << (4))};
enum { R_3NAN_SNAN_SHIFT = (6)}; enum { R_3NAN_SNAN_LENGTH = (1)}; enum { R_3NAN_SNAN_MASK = (((~0ULL) >> (64 - (1))) << (6))};
typedef enum __attribute__((__packed__)) {
    float_3nan_prop_none = 0,
    float_3nan_prop_abc = ((0 << R_3NAN_1ST_SHIFT) | (1 << R_3NAN_2ND_SHIFT) | (2 << R_3NAN_3RD_SHIFT)),
    float_3nan_prop_acb = ((0 << R_3NAN_1ST_SHIFT) | (2 << R_3NAN_2ND_SHIFT) | (1 << R_3NAN_3RD_SHIFT)),
    float_3nan_prop_bac = ((1 << R_3NAN_1ST_SHIFT) | (0 << R_3NAN_2ND_SHIFT) | (2 << R_3NAN_3RD_SHIFT)),
    float_3nan_prop_bca = ((1 << R_3NAN_1ST_SHIFT) | (2 << R_3NAN_2ND_SHIFT) | (0 << R_3NAN_3RD_SHIFT)),
    float_3nan_prop_cab = ((2 << R_3NAN_1ST_SHIFT) | (0 << R_3NAN_2ND_SHIFT) | (1 << R_3NAN_3RD_SHIFT)),
    float_3nan_prop_cba = ((2 << R_3NAN_1ST_SHIFT) | (1 << R_3NAN_2ND_SHIFT) | (0 << R_3NAN_3RD_SHIFT)),
    float_3nan_prop_s_abc = float_3nan_prop_abc | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_acb = float_3nan_prop_acb | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_bac = float_3nan_prop_bac | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_bca = float_3nan_prop_bca | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_cab = float_3nan_prop_cab | R_3NAN_SNAN_MASK,
    float_3nan_prop_s_cba = float_3nan_prop_cba | R_3NAN_SNAN_MASK,
} Float3NaNPropRule;
typedef enum __attribute__((__packed__)) {
    float_infzeronan_none = 0,
    float_infzeronan_dnan_never,
    float_infzeronan_dnan_always,
    float_infzeronan_dnan_if_qnan,
} FloatInfZeroNaNRule;
typedef struct float_status {
    uint16_t float_exception_flags;
    FloatRoundMode float_rounding_mode;
    FloatX80RoundPrec floatx80_rounding_precision;
    Float2NaNPropRule float_2nan_prop_rule;
    Float3NaNPropRule float_3nan_prop_rule;
    FloatInfZeroNaNRule float_infzeronan_rule;
    
   _Bool 
        tininess_before_rounding;
    
   _Bool 
        flush_to_zero;
    
   _Bool 
        flush_inputs_to_zero;
    
   _Bool 
        default_nan_mode;
    uint8_t default_nan_pattern;
    
   _Bool 
        snan_bit_is_one;
    
   _Bool 
        no_signaling_nans;
    
   _Bool 
        rebias_overflow;
    
   _Bool 
        rebias_underflow;
} float_status;
typedef union {
    float32 f;
    uint32_t l;
} CPU_FloatU;
typedef union {
    float64 d;
    struct {
        uint32_t lower;
        uint32_t upper;
    } l;
    uint64_t ll;
} CPU_DoubleU;
typedef union {
     floatx80 d;
     struct {
         uint64_t lower;
         uint16_t upper;
     } l;
} CPU_LDoubleU;
typedef union {
    float128 q;
    struct {
        uint32_t lowest;
        uint32_t lower;
        uint32_t upper;
        uint32_t upmost;
    } l;
    struct {
        uint64_t lower;
        uint64_t upper;
    } ll;
} CPU_QuadU;
typedef struct GDBFeature {
    const char *xmlname;
    const char *xml;
    const char *name;
    const char * const *regs;
    int num_regs;
} GDBFeature;
typedef struct GDBFeatureBuilder {
    GDBFeature *feature;
    GPtrArray *xml;
    GPtrArray *regs;
    int base_reg;
} GDBFeatureBuilder;
typedef int (*gdb_get_reg_cb)(CPUState *cpu, GByteArray *buf, int reg);
typedef int (*gdb_set_reg_cb)(CPUState *cpu, uint8_t *buf, int reg);
void gdb_init_cpu(CPUState *cpu);
void gdb_register_coprocessor(CPUState *cpu,
                              gdb_get_reg_cb get_reg, gdb_set_reg_cb set_reg,
                              const GDBFeature *feature, int g_pos);
void gdb_unregister_coprocessor_all(CPUState *cpu);
_Bool 
    gdbserver_start(const char *port_or_device, Error **errp);
void gdb_feature_builder_init(GDBFeatureBuilder *builder, GDBFeature *feature,
                              const char *name, const char *xmlname,
                              int base_reg);
void __attribute__((__format__ (gnu_printf, 2, 3)))
gdb_feature_builder_append_tag(const GDBFeatureBuilder *builder,
                               const char *format, ...);
void gdb_feature_builder_append_reg(const GDBFeatureBuilder *builder,
                                    const char *name,
                                    int bitsize,
                                    int regnum,
                                    const char *type,
                                    const char *group);
void gdb_feature_builder_end(const GDBFeatureBuilder *builder);
const GDBFeature *gdb_find_static_feature(const char *xmlname);
int gdb_read_register(CPUState *cpu, GByteArray *buf, int reg);
typedef struct {
    int gdb_reg;
    const char *name;
    const char *feature_name;
} GDBRegDesc;
GArray *gdb_get_register_list(CPUState *cpu);
void gdb_set_stop_cpu(CPUState *cpu);
extern const GDBFeature gdb_static_features[];
typedef struct ArchCPU PowerPCCPU; typedef struct ArchCPU ArchCPU; typedef struct PowerPCCPUClass PowerPCCPUClass; typedef ArchCPU *ArchCPU_autoptr; typedef GList *ArchCPU_listautoptr; typedef GSList *ArchCPU_slistautoptr; typedef GQueue *ArchCPU_queueautoptr;
typedef struct PPCTimebase {
    uint64_t guest_timebase;
    int64_t time_of_the_day_ns;
    
   _Bool 
        runstate_paused;
} PPCTimebase;
extern const VMStateDescription vmstate_ppc_timebase;
void cpu_ppc_clock_vm_state_change(void *opaque, 
                                                _Bool 
                                                     running,
                                   RunState state);
enum {
    POWERPC_EXCP_NONE = -1,
    POWERPC_EXCP_CRITICAL = 0,
    POWERPC_EXCP_MCHECK = 1,
    POWERPC_EXCP_DSI = 2,
    POWERPC_EXCP_ISI = 3,
    POWERPC_EXCP_EXTERNAL = 4,
    POWERPC_EXCP_ALIGN = 5,
    POWERPC_EXCP_PROGRAM = 6,
    POWERPC_EXCP_FPU = 7,
    POWERPC_EXCP_SYSCALL = 8,
    POWERPC_EXCP_APU = 9,
    POWERPC_EXCP_DECR = 10,
    POWERPC_EXCP_FIT = 11,
    POWERPC_EXCP_WDT = 12,
    POWERPC_EXCP_DTLB = 13,
    POWERPC_EXCP_ITLB = 14,
    POWERPC_EXCP_DEBUG = 15,
    POWERPC_EXCP_SPEU = 32,
    POWERPC_EXCP_EFPDI = 33,
    POWERPC_EXCP_EFPRI = 34,
    POWERPC_EXCP_EPERFM = 35,
    POWERPC_EXCP_DOORI = 36,
    POWERPC_EXCP_DOORCI = 37,
    POWERPC_EXCP_GDOORI = 38,
    POWERPC_EXCP_GDOORCI = 39,
    POWERPC_EXCP_HYPPRIV = 41,
    POWERPC_EXCP_RESET = 64,
    POWERPC_EXCP_DSEG = 65,
    POWERPC_EXCP_ISEG = 66,
    POWERPC_EXCP_HDECR = 67,
    POWERPC_EXCP_TRACE = 68,
    POWERPC_EXCP_HDSI = 69,
    POWERPC_EXCP_HISI = 70,
    POWERPC_EXCP_HDSEG = 71,
    POWERPC_EXCP_HISEG = 72,
    POWERPC_EXCP_VPU = 73,
    POWERPC_EXCP_PIT = 74,
    POWERPC_EXCP_EMUL = 77,
    POWERPC_EXCP_IFTLB = 78,
    POWERPC_EXCP_DLTLB = 79,
    POWERPC_EXCP_DSTLB = 80,
    POWERPC_EXCP_FPA = 81,
    POWERPC_EXCP_DABR = 82,
    POWERPC_EXCP_IABR = 83,
    POWERPC_EXCP_SMI = 84,
    POWERPC_EXCP_PERFM = 85,
    POWERPC_EXCP_THERM = 86,
    POWERPC_EXCP_VPUA = 87,
    POWERPC_EXCP_SOFTP = 88,
    POWERPC_EXCP_MAINT = 89,
    POWERPC_EXCP_MEXTBR = 90,
    POWERPC_EXCP_NMEXTBR = 91,
    POWERPC_EXCP_ITLBE = 92,
    POWERPC_EXCP_DTLBE = 93,
    POWERPC_EXCP_VSXU = 94,
    POWERPC_EXCP_FU = 95,
    POWERPC_EXCP_HV_EMU = 96,
    POWERPC_EXCP_HV_MAINT = 97,
    POWERPC_EXCP_HV_FU = 98,
    POWERPC_EXCP_SDOOR = 99,
    POWERPC_EXCP_SDOOR_HV = 100,
    POWERPC_EXCP_HVIRT = 101,
    POWERPC_EXCP_SYSCALL_VECTORED = 102,
    POWERPC_EXCP_PERFM_EBB = 103,
    POWERPC_EXCP_EXTERNAL_EBB = 104,
    POWERPC_EXCP_NB = 105,
    POWERPC_EXCP_SYSCALL_USER = 0x203,
};
enum {
    POWERPC_EXCP_ALIGN_FP = 0x01,
    POWERPC_EXCP_ALIGN_LST = 0x02,
    POWERPC_EXCP_ALIGN_LE = 0x03,
    POWERPC_EXCP_ALIGN_PROT = 0x04,
    POWERPC_EXCP_ALIGN_BAT = 0x05,
    POWERPC_EXCP_ALIGN_CACHE = 0x06,
    POWERPC_EXCP_ALIGN_INSN = 0x07,
    POWERPC_EXCP_FP = 0x10,
    POWERPC_EXCP_FP_OX = 0x01,
    POWERPC_EXCP_FP_UX = 0x02,
    POWERPC_EXCP_FP_ZX = 0x03,
    POWERPC_EXCP_FP_XX = 0x04,
    POWERPC_EXCP_FP_VXSNAN = 0x05,
    POWERPC_EXCP_FP_VXISI = 0x06,
    POWERPC_EXCP_FP_VXIDI = 0x07,
    POWERPC_EXCP_FP_VXZDZ = 0x08,
    POWERPC_EXCP_FP_VXIMZ = 0x09,
    POWERPC_EXCP_FP_VXVC = 0x0A,
    POWERPC_EXCP_FP_VXSOFT = 0x0B,
    POWERPC_EXCP_FP_VXSQRT = 0x0C,
    POWERPC_EXCP_FP_VXCVI = 0x0D,
    POWERPC_EXCP_INVAL = 0x20,
    POWERPC_EXCP_INVAL_INVAL = 0x01,
    POWERPC_EXCP_INVAL_LSWX = 0x02,
    POWERPC_EXCP_INVAL_SPR = 0x03,
    POWERPC_EXCP_INVAL_FP = 0x04,
    POWERPC_EXCP_PRIV = 0x30,
    POWERPC_EXCP_PRIV_OPC = 0x01,
    POWERPC_EXCP_PRIV_REG = 0x02,
    POWERPC_EXCP_TRAP = 0x40,
};
typedef enum powerpc_excp_t {
    POWERPC_EXCP_UNKNOWN = 0,
    POWERPC_EXCP_STD,
    POWERPC_EXCP_40x,
    POWERPC_EXCP_6xx,
    POWERPC_EXCP_7xx,
    POWERPC_EXCP_74xx,
    POWERPC_EXCP_BOOKE,
    POWERPC_EXCP_970,
    POWERPC_EXCP_POWER7,
    POWERPC_EXCP_POWER8,
    POWERPC_EXCP_POWER9,
    POWERPC_EXCP_POWER10,
    POWERPC_EXCP_POWER11,
} powerpc_excp_t;
typedef enum powerpc_mmu_t {
    POWERPC_MMU_UNKNOWN = 0x00000000,
    POWERPC_MMU_32B = 0x00000001,
    POWERPC_MMU_SOFT_6xx = 0x00000002,
    POWERPC_MMU_SOFT_74xx = 0x00000003,
    POWERPC_MMU_SOFT_4xx = 0x00000004,
    POWERPC_MMU_REAL = 0x00000006,
    POWERPC_MMU_MPC8xx = 0x00000007,
    POWERPC_MMU_BOOKE = 0x00000008,
    POWERPC_MMU_BOOKE206 = 0x00000009,
    POWERPC_MMU_64B = 0x00010000 | 0x00000001,
    POWERPC_MMU_2_03 = 0x00010000 | 0x00000002,
    POWERPC_MMU_2_06 = 0x00010000 | 0x00000003,
    POWERPC_MMU_2_07 = 0x00010000 | 0x00000004,
    POWERPC_MMU_3_00 = 0x00010000 | 0x00000005,
} powerpc_mmu_t;
typedef enum powerpc_input_t {
    PPC_FLAGS_INPUT_UNKNOWN = 0,
    PPC_FLAGS_INPUT_6xx,
    PPC_FLAGS_INPUT_BookE,
    PPC_FLAGS_INPUT_405,
    PPC_FLAGS_INPUT_970,
    PPC_FLAGS_INPUT_POWER7,
    PPC_FLAGS_INPUT_POWER9,
    PPC_FLAGS_INPUT_RCPU,
} powerpc_input_t;
typedef struct opc_handler_t opc_handler_t;
typedef struct DisasContext DisasContext;
typedef struct ppc_dcr_t ppc_dcr_t;
typedef struct ppc_spr_t ppc_spr_t;
typedef struct ppc_tb_t ppc_tb_t;
typedef union ppc_tlb_t ppc_tlb_t;
typedef struct ppc_hash_pte64 ppc_hash_pte64_t;
typedef struct PPCHash64Options PPCHash64Options;
typedef struct CPUArchState CPUPPCState;
struct ppc_spr_t {
    const char *name;
    target_ulong default_value;
    unsigned int gdb_id;
    void (*uea_read)(DisasContext *ctx, int gpr_num, int spr_num);
    void (*uea_write)(DisasContext *ctx, int spr_num, int gpr_num);
    void (*oea_read)(DisasContext *ctx, int gpr_num, int spr_num);
    void (*oea_write)(DisasContext *ctx, int spr_num, int gpr_num);
    void (*hea_read)(DisasContext *ctx, int gpr_num, int spr_num);
    void (*hea_write)(DisasContext *ctx, int spr_num, int gpr_num);
};
typedef union _ppc_vsr_t {
    uint8_t u8[16];
    uint16_t u16[8];
    uint32_t u32[4];
    uint64_t u64[2];
    int8_t s8[16];
    int16_t s16[8];
    int32_t s32[4];
    int64_t s64[2];
    float16 f16[8];
    float32 f32[4];
    float64 f64[2];
    float128 f128;
    __uint128_t u128;
    Int128 s128;
} ppc_vsr_t;
typedef ppc_vsr_t ppc_avr_t;
typedef ppc_vsr_t ppc_fprp_t;
typedef ppc_vsr_t ppc_acc_t;
typedef struct ppc6xx_tlb_t ppc6xx_tlb_t;
struct ppc6xx_tlb_t {
    target_ulong pte0;
    target_ulong pte1;
    target_ulong EPN;
};
typedef struct ppcemb_tlb_t ppcemb_tlb_t;
struct ppcemb_tlb_t {
    uint64_t RPN;
    target_ulong EPN;
    target_ulong PID;
    target_ulong size;
    uint32_t prot;
    uint32_t attr;
};
typedef struct ppcmas_tlb_t {
     uint32_t mas8;
     uint32_t mas1;
     uint64_t mas2;
     uint64_t mas7_3;
} ppcmas_tlb_t;
union ppc_tlb_t {
    ppc6xx_tlb_t *tlb6;
    ppcemb_tlb_t *tlbe;
    ppcmas_tlb_t *tlbm;
};
typedef struct PPCHash64SegmentPageSizes PPCHash64SegmentPageSizes;
typedef struct ppc_slb_t ppc_slb_t;
struct ppc_slb_t {
    uint64_t esid;
    uint64_t vsid;
    const PPCHash64SegmentPageSizes *sps;
};
typedef struct ppc_v3_pate_t {
    uint64_t dw0;
    uint64_t dw1;
} ppc_v3_pate_t;
typedef enum {
    PMU_EVENT_INVALID = 0,
    PMU_EVENT_INACTIVE,
    PMU_EVENT_CYCLES,
    PMU_EVENT_INSTRUCTIONS,
    PMU_EVENT_INSN_RUN_LATCH,
} PMUEventType;
enum { R_MSR_SF_SHIFT = ((63 - (0)))}; enum { R_MSR_SF_LENGTH = (1)}; enum { R_MSR_SF_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (0))))};
enum { R_MSR_TAG_SHIFT = ((63 - (1)))}; enum { R_MSR_TAG_LENGTH = (1)}; enum { R_MSR_TAG_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (1))))};
enum { R_MSR_ISF_SHIFT = ((63 - (2)))}; enum { R_MSR_ISF_LENGTH = (1)}; enum { R_MSR_ISF_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (2))))};
enum { R_MSR_TS0_SHIFT = ((63 - (29)))}; enum { R_MSR_TS0_LENGTH = (1)}; enum { R_MSR_TS0_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (29))))};
enum { R_MSR_TS1_SHIFT = ((63 - (30)))}; enum { R_MSR_TS1_LENGTH = (1)}; enum { R_MSR_TS1_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (30))))};
enum { R_MSR_TS_SHIFT = ((63 - (29)))}; enum { R_MSR_TS_LENGTH = (2)}; enum { R_MSR_TS_MASK = (((~0ULL) >> (64 - (2))) << ((63 - (29))))};
enum { R_MSR_TM_SHIFT = ((63 - (31)))}; enum { R_MSR_TM_LENGTH = (1)}; enum { R_MSR_TM_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (31))))};
enum { R_MSR_CM_SHIFT = ((63 - (32)))}; enum { R_MSR_CM_LENGTH = (1)}; enum { R_MSR_CM_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (32))))};
enum { R_MSR_ICM_SHIFT = ((63 - (33)))}; enum { R_MSR_ICM_LENGTH = (1)}; enum { R_MSR_ICM_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (33))))};
enum { R_MSR_GS_SHIFT = ((63 - (35)))}; enum { R_MSR_GS_LENGTH = (1)}; enum { R_MSR_GS_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (35))))};
enum { R_MSR_UCLE_SHIFT = ((63 - (37)))}; enum { R_MSR_UCLE_LENGTH = (1)}; enum { R_MSR_UCLE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (37))))};
enum { R_MSR_VR_SHIFT = ((63 - (38)))}; enum { R_MSR_VR_LENGTH = (1)}; enum { R_MSR_VR_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (38))))};
enum { R_MSR_SPE_SHIFT = ((63 - (38)))}; enum { R_MSR_SPE_LENGTH = (1)}; enum { R_MSR_SPE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (38))))};
enum { R_MSR_VSX_SHIFT = ((63 - (40)))}; enum { R_MSR_VSX_LENGTH = (1)}; enum { R_MSR_VSX_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (40))))};
enum { R_MSR_S_SHIFT = ((63 - (41)))}; enum { R_MSR_S_LENGTH = (1)}; enum { R_MSR_S_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (41))))};
enum { R_MSR_KEY_SHIFT = ((63 - (44)))}; enum { R_MSR_KEY_LENGTH = (1)}; enum { R_MSR_KEY_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (44))))};
enum { R_MSR_POW_SHIFT = ((63 - (45)))}; enum { R_MSR_POW_LENGTH = (1)}; enum { R_MSR_POW_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (45))))};
enum { R_MSR_WE_SHIFT = ((63 - (45)))}; enum { R_MSR_WE_LENGTH = (1)}; enum { R_MSR_WE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (45))))};
enum { R_MSR_TGPR_SHIFT = ((63 - (46)))}; enum { R_MSR_TGPR_LENGTH = (1)}; enum { R_MSR_TGPR_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (46))))};
enum { R_MSR_CE_SHIFT = ((63 - (46)))}; enum { R_MSR_CE_LENGTH = (1)}; enum { R_MSR_CE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (46))))};
enum { R_MSR_ILE_SHIFT = ((63 - (47)))}; enum { R_MSR_ILE_LENGTH = (1)}; enum { R_MSR_ILE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (47))))};
enum { R_MSR_EE_SHIFT = ((63 - (48)))}; enum { R_MSR_EE_LENGTH = (1)}; enum { R_MSR_EE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (48))))};
enum { R_MSR_PR_SHIFT = ((63 - (49)))}; enum { R_MSR_PR_LENGTH = (1)}; enum { R_MSR_PR_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (49))))};
enum { R_MSR_FP_SHIFT = ((63 - (50)))}; enum { R_MSR_FP_LENGTH = (1)}; enum { R_MSR_FP_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (50))))};
enum { R_MSR_ME_SHIFT = ((63 - (51)))}; enum { R_MSR_ME_LENGTH = (1)}; enum { R_MSR_ME_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (51))))};
enum { R_MSR_FE0_SHIFT = ((63 - (52)))}; enum { R_MSR_FE0_LENGTH = (1)}; enum { R_MSR_FE0_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (52))))};
enum { R_MSR_SE_SHIFT = ((63 - (53)))}; enum { R_MSR_SE_LENGTH = (1)}; enum { R_MSR_SE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (53))))};
enum { R_MSR_DWE_SHIFT = ((63 - (53)))}; enum { R_MSR_DWE_LENGTH = (1)}; enum { R_MSR_DWE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (53))))};
enum { R_MSR_UBLE_SHIFT = ((63 - (53)))}; enum { R_MSR_UBLE_LENGTH = (1)}; enum { R_MSR_UBLE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (53))))};
enum { R_MSR_BE_SHIFT = ((63 - (54)))}; enum { R_MSR_BE_LENGTH = (1)}; enum { R_MSR_BE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (54))))};
enum { R_MSR_DE_SHIFT = ((63 - (54)))}; enum { R_MSR_DE_LENGTH = (1)}; enum { R_MSR_DE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (54))))};
enum { R_MSR_FE1_SHIFT = ((63 - (55)))}; enum { R_MSR_FE1_LENGTH = (1)}; enum { R_MSR_FE1_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (55))))};
enum { R_MSR_AL_SHIFT = ((63 - (56)))}; enum { R_MSR_AL_LENGTH = (1)}; enum { R_MSR_AL_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (56))))};
enum { R_MSR_EP_SHIFT = ((63 - (57)))}; enum { R_MSR_EP_LENGTH = (1)}; enum { R_MSR_EP_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (57))))};
enum { R_MSR_IR_SHIFT = ((63 - (58)))}; enum { R_MSR_IR_LENGTH = (1)}; enum { R_MSR_IR_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (58))))};
enum { R_MSR_DR_SHIFT = ((63 - (59)))}; enum { R_MSR_DR_LENGTH = (1)}; enum { R_MSR_DR_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (59))))};
enum { R_MSR_IS_SHIFT = ((63 - (58)))}; enum { R_MSR_IS_LENGTH = (1)}; enum { R_MSR_IS_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (58))))};
enum { R_MSR_DS_SHIFT = ((63 - (59)))}; enum { R_MSR_DS_LENGTH = (1)}; enum { R_MSR_DS_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (59))))};
enum { R_MSR_PE_SHIFT = ((63 - (60)))}; enum { R_MSR_PE_LENGTH = (1)}; enum { R_MSR_PE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (60))))};
enum { R_MSR_PX_SHIFT = ((63 - (61)))}; enum { R_MSR_PX_LENGTH = (1)}; enum { R_MSR_PX_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (61))))};
enum { R_MSR_PMM_SHIFT = ((63 - (61)))}; enum { R_MSR_PMM_LENGTH = (1)}; enum { R_MSR_PMM_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (61))))};
enum { R_MSR_RI_SHIFT = ((63 - (62)))}; enum { R_MSR_RI_LENGTH = (1)}; enum { R_MSR_RI_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (62))))};
enum { R_MSR_LE_SHIFT = ((63 - (63)))}; enum { R_MSR_LE_LENGTH = (1)}; enum { R_MSR_LE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (63))))};
enum {
    POWERPC_FLAG_NONE = 0x00000000,
    POWERPC_FLAG_SPE = 0x00000001,
    POWERPC_FLAG_VRE = 0x00000002,
    POWERPC_FLAG_TGPR = 0x00000004,
    POWERPC_FLAG_CE = 0x00000008,
    POWERPC_FLAG_SE = 0x00000010,
    POWERPC_FLAG_DWE = 0x00000020,
    POWERPC_FLAG_UBLE = 0x00000040,
    POWERPC_FLAG_BE = 0x00000080,
    POWERPC_FLAG_DE = 0x00000100,
    POWERPC_FLAG_PX = 0x00000200,
    POWERPC_FLAG_PMM = 0x00000400,
    POWERPC_FLAG_BUS_CLK = 0x00020000,
    POWERPC_FLAG_CFAR = 0x00040000,
    POWERPC_FLAG_VSX = 0x00080000,
    POWERPC_FLAG_TM = 0x00100000,
    POWERPC_FLAG_SCV = 0x00200000,
    POWERPC_FLAG_SMT = 0x00400000,
    POWERPC_FLAG_SMT_1LPAR = 0x00800000,
    POWERPC_FLAG_BHRB = 0x01000000,
};
enum {
    HFLAGS_LE = 0,
    HFLAGS_HV = 1,
    HFLAGS_64 = 2,
    HFLAGS_GTSE = 3,
    HFLAGS_DR = 4,
    HFLAGS_HR = 5,
    HFLAGS_SPE = 6,
    HFLAGS_TM = 8,
    HFLAGS_BE = 9,
    HFLAGS_SE = 10,
    HFLAGS_FP = 13,
    HFLAGS_PR = 14,
    HFLAGS_PMCC0 = 15,
    HFLAGS_PMCC1 = 16,
    HFLAGS_PMCJCE = 17,
    HFLAGS_PMC_OTHER = 18,
    HFLAGS_INSN_CNT = 19,
    HFLAGS_BHRB_ENABLE = 20,
    HFLAGS_VSX = 23,
    HFLAGS_VR = 25,
    HFLAGS_IMMU_IDX = 26,
    HFLAGS_DMMU_IDX = 29,
};
enum { R_FPSCR_FI_SHIFT = ((63 - (46)))}; enum { R_FPSCR_FI_LENGTH = (1)}; enum { R_FPSCR_FI_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (46))))};
struct ppc_radix_page_info {
    uint32_t count;
    uint32_t entries[8];
};
enum { R_DEXCR_PNH_SBHE_SHIFT = ((63 - (0)))}; enum { R_DEXCR_PNH_SBHE_LENGTH = (1)}; enum { R_DEXCR_PNH_SBHE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (0))))}; enum { R_DEXCR_PRO_SBHE_SHIFT = ((63 - (0 + 32)))}; enum { R_DEXCR_PRO_SBHE_LENGTH = (1)}; enum { R_DEXCR_PRO_SBHE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (0 + 32))))}; enum { R_HDEXCR_HNU_SBHE_SHIFT = ((63 - (0)))}; enum { R_HDEXCR_HNU_SBHE_LENGTH = (1)}; enum { R_HDEXCR_HNU_SBHE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (0))))}; enum { R_HDEXCR_ENF_SBHE_SHIFT = ((63 - (0 + 32)))}; enum { R_HDEXCR_ENF_SBHE_LENGTH = (1)}; enum { R_HDEXCR_ENF_SBHE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (0 + 32))))};
enum { R_DEXCR_PNH_IBRTPD_SHIFT = ((63 - (1)))}; enum { R_DEXCR_PNH_IBRTPD_LENGTH = (1)}; enum { R_DEXCR_PNH_IBRTPD_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (1))))}; enum { R_DEXCR_PRO_IBRTPD_SHIFT = ((63 - (1 + 32)))}; enum { R_DEXCR_PRO_IBRTPD_LENGTH = (1)}; enum { R_DEXCR_PRO_IBRTPD_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (1 + 32))))}; enum { R_HDEXCR_HNU_IBRTPD_SHIFT = ((63 - (1)))}; enum { R_HDEXCR_HNU_IBRTPD_LENGTH = (1)}; enum { R_HDEXCR_HNU_IBRTPD_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (1))))}; enum { R_HDEXCR_ENF_IBRTPD_SHIFT = ((63 - (1 + 32)))}; enum { R_HDEXCR_ENF_IBRTPD_LENGTH = (1)}; enum { R_HDEXCR_ENF_IBRTPD_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (1 + 32))))};
enum { R_DEXCR_PNH_SRAPD_SHIFT = ((63 - (4)))}; enum { R_DEXCR_PNH_SRAPD_LENGTH = (1)}; enum { R_DEXCR_PNH_SRAPD_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (4))))}; enum { R_DEXCR_PRO_SRAPD_SHIFT = ((63 - (4 + 32)))}; enum { R_DEXCR_PRO_SRAPD_LENGTH = (1)}; enum { R_DEXCR_PRO_SRAPD_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (4 + 32))))}; enum { R_HDEXCR_HNU_SRAPD_SHIFT = ((63 - (4)))}; enum { R_HDEXCR_HNU_SRAPD_LENGTH = (1)}; enum { R_HDEXCR_HNU_SRAPD_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (4))))}; enum { R_HDEXCR_ENF_SRAPD_SHIFT = ((63 - (4 + 32)))}; enum { R_HDEXCR_ENF_SRAPD_LENGTH = (1)}; enum { R_HDEXCR_ENF_SRAPD_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (4 + 32))))};
enum { R_DEXCR_PNH_NPHIE_SHIFT = ((63 - (5)))}; enum { R_DEXCR_PNH_NPHIE_LENGTH = (1)}; enum { R_DEXCR_PNH_NPHIE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (5))))}; enum { R_DEXCR_PRO_NPHIE_SHIFT = ((63 - (5 + 32)))}; enum { R_DEXCR_PRO_NPHIE_LENGTH = (1)}; enum { R_DEXCR_PRO_NPHIE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (5 + 32))))}; enum { R_HDEXCR_HNU_NPHIE_SHIFT = ((63 - (5)))}; enum { R_HDEXCR_HNU_NPHIE_LENGTH = (1)}; enum { R_HDEXCR_HNU_NPHIE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (5))))}; enum { R_HDEXCR_ENF_NPHIE_SHIFT = ((63 - (5 + 32)))}; enum { R_HDEXCR_ENF_NPHIE_LENGTH = (1)}; enum { R_HDEXCR_ENF_NPHIE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (5 + 32))))};
enum { R_DEXCR_PNH_PHIE_SHIFT = ((63 - (6)))}; enum { R_DEXCR_PNH_PHIE_LENGTH = (1)}; enum { R_DEXCR_PNH_PHIE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (6))))}; enum { R_DEXCR_PRO_PHIE_SHIFT = ((63 - (6 + 32)))}; enum { R_DEXCR_PRO_PHIE_LENGTH = (1)}; enum { R_DEXCR_PRO_PHIE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (6 + 32))))}; enum { R_HDEXCR_HNU_PHIE_SHIFT = ((63 - (6)))}; enum { R_HDEXCR_HNU_PHIE_LENGTH = (1)}; enum { R_HDEXCR_HNU_PHIE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (6))))}; enum { R_HDEXCR_ENF_PHIE_SHIFT = ((63 - (6 + 32)))}; enum { R_HDEXCR_ENF_PHIE_LENGTH = (1)}; enum { R_HDEXCR_ENF_PHIE_MASK = (((~0ULL) >> (64 - (1))) << ((63 - (6 + 32))))};
struct CPUArchState {
    target_ulong gpr[32];
    target_ulong gprh[32];
    target_ulong lr;
    target_ulong ctr;
    uint32_t crf[8];
    target_ulong xer;
    target_ulong so;
    target_ulong ov;
    target_ulong ca;
    target_ulong ov32;
    target_ulong ca32;
    target_ulong reserve_addr;
    target_ulong reserve_length;
    target_ulong reserve_val;
    target_ulong msr;
    target_ulong tgpr[4];
    target_ulong nip;
    int access_type;
    
   _Bool 
        has_smt_siblings;
    int core_index;
    int chip_index;
    target_ulong sr[32];
    uint32_t nb_BATs;
    target_ulong DBAT[2][8];
    target_ulong IBAT[2][8];
    int32_t nb_tlb;
    int tlb_per_way;
    int nb_ways;
    int last_way;
    int nb_pids;
    int tlb_type;
    ppc_tlb_t tlb;
    uint32_t tlb_need_flush;
    target_ulong spr[1024];
    ppc_spr_t spr_cb[1024];
    uint8_t pmc_ins_cnt;
    uint8_t pmc_cyc_cnt;
    uint32_t vscr;
    ppc_vsr_t vsr[64] __attribute__((aligned(16)));
    ppc_vsr_t vscr_sat __attribute__((aligned(16)));
    uint64_t spe_acc;
    uint32_t spe_fscr;
    float_status vec_status;
    float_status fp_status;
    target_ulong fpscr;
    ppc_tb_t *tb_env;
    ppc_dcr_t *dcr_env;
    int dcache_line_size;
    int icache_line_size;
    target_ulong msr_mask;
    powerpc_mmu_t mmu_model;
    powerpc_excp_t excp_model;
    powerpc_input_t bus_model;
    int bfd_mach;
    uint32_t flags;
    uint64_t insns_flags;
    uint64_t insns_flags2;
    int error_code;
    uint32_t pending_interrupts;
    uint64_t excp_stats[POWERPC_EXCP_NB];
    uint32_t irq_input_state;
    target_ulong excp_vectors[POWERPC_EXCP_NB];
    target_ulong excp_prefix;
    target_ulong ivor_mask;
    target_ulong ivpr_mask;
    target_ulong hreset_vector;
    hwaddr mpic_iack;
    
   _Bool 
        mpic_proxy;
    
   _Bool 
        has_hv_mode;
    
   _Bool 
        resume_as_sreset;
    
   _Bool 
        quiesced;
    uint32_t hflags;
    target_ulong hflags_compat_nmsr;
    int (*check_pow)(CPUPPCState *env);
    int (*check_attn)(CPUPPCState *env);
    void *load_info;
    uint8_t fit_period[4];
    uint8_t wdt_period[4];
    target_ulong tm_gpr[32];
    ppc_avr_t tm_vsr[64];
    uint64_t tm_cr;
    uint64_t tm_lr;
    uint64_t tm_ctr;
    uint64_t tm_fpscr;
    uint64_t tm_amr;
    uint64_t tm_ppr;
    uint64_t tm_vrsave;
    uint32_t tm_vscr;
    uint64_t tm_dscr;
    uint64_t tm_tar;
    QEMUTimer *pmu_cyc_overflow_timers[6];
    uint64_t pmu_base_time;
};
typedef struct PPCVirtualHypervisor PPCVirtualHypervisor;
typedef struct PPCVirtualHypervisorClass PPCVirtualHypervisorClass;
struct ArchCPU {
    CPUState parent_obj;
    CPUPPCState env;
    int vcpu_id;
    uint32_t compat_pvr;
    PPCVirtualHypervisor *vhyp;
    PPCVirtualHypervisorClass *vhyp_class;
    void *machine_data;
    int32_t node_id;
    PPCHash64Options *hash64_opts;
    opc_handler_t *opcodes[0x40];
};
struct PowerPCCPUClass {
    CPUClass parent_class;
    DeviceRealize parent_realize;
    DeviceUnrealize parent_unrealize;
    ResettablePhases parent_phases;
    void (*parent_parse_features)(const char *type, char *str, Error **errp);
    uint32_t pvr;
    uint32_t spapr_logical_pvr;
    
   _Bool 
        (*pvr_match)(struct PowerPCCPUClass *pcc, uint32_t pvr, 
                                                                _Bool 
                                                                     best);
    uint64_t pcr_mask;
    uint64_t pcr_supported;
    uint32_t svr;
    uint64_t insns_flags;
    uint64_t insns_flags2;
    uint64_t msr_mask;
    uint64_t lpcr_mask;
    uint64_t lpcr_pm;
    powerpc_mmu_t mmu_model;
    powerpc_excp_t excp_model;
    powerpc_input_t bus_model;
    uint32_t flags;
    int bfd_mach;
    uint32_t l1_dcache_size, l1_icache_size;
    GDBFeature gdb_spr;
    const PPCHash64Options *hash64_opts;
    struct ppc_radix_page_info *radix_page_info;
    uint32_t lrg_decr_bits;
    int n_host_threads;
    void (*init_proc)(CPUPPCState *env);
    int (*check_pow)(CPUPPCState *env);
    int (*check_attn)(CPUPPCState *env);
};
ObjectClass *ppc_cpu_class_by_name(const char *name);
PowerPCCPUClass *ppc_cpu_class_by_pvr(uint32_t pvr);
PowerPCCPUClass *ppc_cpu_class_by_pvr_mask(uint32_t pvr);
PowerPCCPUClass *ppc_cpu_get_family_class(PowerPCCPUClass *pcc);
struct PPCVirtualHypervisorClass {
    InterfaceClass parent;
    
   _Bool 
        (*cpu_in_nested)(PowerPCCPU *cpu);
    void (*deliver_hv_excp)(PowerPCCPU *cpu, int excp);
    void (*hypercall)(PPCVirtualHypervisor *vhyp, PowerPCCPU *cpu);
    hwaddr (*hpt_mask)(PPCVirtualHypervisor *vhyp);
    const ppc_hash_pte64_t *(*map_hptes)(PPCVirtualHypervisor *vhyp,
                                         hwaddr ptex, int n);
    void (*unmap_hptes)(PPCVirtualHypervisor *vhyp,
                        const ppc_hash_pte64_t *hptes,
                        hwaddr ptex, int n);
    void (*hpte_set_c)(PPCVirtualHypervisor *vhyp, hwaddr ptex, uint64_t pte1);
    void (*hpte_set_r)(PPCVirtualHypervisor *vhyp, hwaddr ptex, uint64_t pte1);
    
   _Bool 
        (*get_pate)(PPCVirtualHypervisor *vhyp, PowerPCCPU *cpu,
                     target_ulong lpid, ppc_v3_pate_t *entry);
    target_ulong (*encode_hpt_for_kvm_pr)(PPCVirtualHypervisor *vhyp);
    void (*cpu_exec_enter)(PPCVirtualHypervisor *vhyp, PowerPCCPU *cpu);
    void (*cpu_exec_exit)(PPCVirtualHypervisor *vhyp, PowerPCCPU *cpu);
};
void ppc_cpu_dump_state(CPUState *cpu, FILE *f, int flags);
int ppc_cpu_gdb_read_register(CPUState *cpu, GByteArray *buf, int reg);
int ppc_cpu_gdb_read_register_apple(CPUState *cpu, GByteArray *buf, int reg);
int ppc_cpu_gdb_write_register(CPUState *cpu, uint8_t *buf, int reg);
int ppc_cpu_gdb_write_register_apple(CPUState *cpu, uint8_t *buf, int reg);
hwaddr ppc_cpu_get_phys_page_debug(CPUState *cpu, vaddr addr);
int ppc64_cpu_write_elf64_note(WriteCoreDumpFunction f, CPUState *cs,
                               int cpuid, DumpState *s);
int ppc32_cpu_write_elf32_note(WriteCoreDumpFunction f, CPUState *cs,
                               int cpuid, DumpState *s);
void ppc_maybe_interrupt(CPUPPCState *env);
void ppc_cpu_do_interrupt(CPUState *cpu);
_Bool 
    ppc_cpu_exec_interrupt(CPUState *cpu, int int_req);
void ppc_cpu_do_system_reset(CPUState *cs);
void ppc_cpu_do_fwnmi_machine_check(CPUState *cs, target_ulong vector);
extern const VMStateDescription vmstate_ppc_cpu;
void ppc_translate_init(void);
void ppc_translate_code(CPUState *cs, TranslationBlock *tb,
                        int *max_insns, vaddr pc, void *host_pc);
void ppc_store_sdr1(CPUPPCState *env, target_ulong value);
void ppc_store_lpcr(PowerPCCPU *cpu, target_ulong val);
void ppc_update_ciabr(CPUPPCState *env);
void ppc_store_ciabr(CPUPPCState *env, target_ulong value);
void ppc_update_daw0(CPUPPCState *env);
void ppc_store_dawr0(CPUPPCState *env, target_ulong value);
void ppc_store_dawrx0(CPUPPCState *env, uint32_t value);
void ppc_store_msr(CPUPPCState *env, target_ulong value);
void ppc_cpu_list(void);
uint64_t cpu_ppc_load_tbl(CPUPPCState *env);
uint32_t cpu_ppc_load_tbu(CPUPPCState *env);
void cpu_ppc_store_tbu(CPUPPCState *env, uint32_t value);
void cpu_ppc_store_tbl(CPUPPCState *env, uint32_t value);
uint64_t cpu_ppc_load_atbl(CPUPPCState *env);
uint32_t cpu_ppc_load_atbu(CPUPPCState *env);
void cpu_ppc_store_atbl(CPUPPCState *env, uint32_t value);
void cpu_ppc_store_atbu(CPUPPCState *env, uint32_t value);
void cpu_ppc_increase_tb_by_offset(CPUPPCState *env, int64_t offset);
void cpu_ppc_decrease_tb_by_offset(CPUPPCState *env, int64_t offset);
uint64_t cpu_ppc_load_vtb(CPUPPCState *env);
void cpu_ppc_store_vtb(CPUPPCState *env, uint64_t value);
_Bool 
    ppc_decr_clear_on_delivery(CPUPPCState *env);
target_ulong cpu_ppc_load_decr(CPUPPCState *env);
void cpu_ppc_store_decr(CPUPPCState *env, target_ulong value);
target_ulong cpu_ppc_load_hdecr(CPUPPCState *env);
void cpu_ppc_store_hdecr(CPUPPCState *env, target_ulong value);
void cpu_ppc_store_tbu40(CPUPPCState *env, uint64_t value);
uint64_t cpu_ppc_load_purr(CPUPPCState *env);
void cpu_ppc_store_purr(CPUPPCState *env, uint64_t value);
target_ulong load_40x_pit(CPUPPCState *env);
void store_40x_pit(CPUPPCState *env, target_ulong val);
void store_40x_dbcr0(CPUPPCState *env, uint32_t val);
void store_40x_sler(CPUPPCState *env, uint32_t val);
void store_40x_tcr(CPUPPCState *env, target_ulong val);
void store_40x_tsr(CPUPPCState *env, target_ulong val);
void store_booke_tcr(CPUPPCState *env, target_ulong val);
void store_booke_tsr(CPUPPCState *env, target_ulong val);
void ppc_tlb_invalidate_all(CPUPPCState *env);
void ppc_tlb_invalidate_one(CPUPPCState *env, target_ulong addr);
void cpu_ppc_set_vhyp(PowerPCCPU *cpu, PPCVirtualHypervisor *vhyp);
void cpu_ppc_set_1lpar(PowerPCCPU *cpu);
void ppc_store_fpscr(CPUPPCState *env, target_ulong val);
void helper_hfscr_facility_check(CPUPPCState *env, uint32_t bit,
                                 const char *caller, uint32_t cause);
int ppc_dcr_read(ppc_dcr_t *dcr_env, int dcrn, uint32_t *valp);
int ppc_dcr_write(ppc_dcr_t *dcr_env, int dcrn, uint32_t val);
typedef struct RAMBlockNotifier RAMBlockNotifier;
typedef struct {
    struct rcu_head rcu;
    unsigned long *blocks[];
} DirtyMemoryBlocks;
typedef struct RAMList {
    QemuMutex mutex;
    RAMBlock *mru_block;
    struct { struct RAMBlock *lh_first; } blocks;
    DirtyMemoryBlocks *dirty_memory[3];
    unsigned int num_dirty_blocks;
    uint32_t version;
    struct { struct RAMBlockNotifier *lh_first; } ramblock_notifiers;
} RAMList;
extern RAMList ram_list;
void qemu_mutex_lock_ramlist(void);
void qemu_mutex_unlock_ramlist(void);
struct RAMBlockNotifier {
    void (*ram_block_added)(RAMBlockNotifier *n, void *host, size_t size,
                            size_t max_size);
    void (*ram_block_removed)(RAMBlockNotifier *n, void *host, size_t size,
                              size_t max_size);
    void (*ram_block_resized)(RAMBlockNotifier *n, void *host, size_t old_size,
                              size_t new_size);
    struct { struct RAMBlockNotifier *le_next; struct RAMBlockNotifier **le_prev; } next;
};
void ram_block_notifier_add(RAMBlockNotifier *n);
void ram_block_notifier_remove(RAMBlockNotifier *n);
void ram_block_notify_add(void *host, size_t size, size_t max_size);
void ram_block_notify_remove(void *host, size_t size, size_t max_size);
void ram_block_notify_resize(void *host, size_t old_size, size_t new_size);
GString *ram_block_format(void);
struct Range {
    uint64_t lob;
    uint64_t upb;
};
int range_compare(Range *a, Range *b);
GList *range_list_insert(GList *list, Range *data);
void range_inverse_array(GList *in_ranges,
                         GList **out_ranges,
                         uint64_t low, uint64_t high);
typedef struct IOMMUMemoryRegionClass IOMMUMemoryRegionClass;
typedef struct RamDiscardManagerClass RamDiscardManagerClass;
typedef struct RamDiscardManager RamDiscardManager;
                                                                   ;
extern unsigned int global_dirty_tracking;
typedef struct MemoryRegionOps MemoryRegionOps;
struct ReservedRegion {
    Range range;
    unsigned type;
};
struct MemoryRegionSection {
    Int128 size;
    MemoryRegion *mr;
    FlatView *fv;
    hwaddr offset_within_region;
    hwaddr offset_within_address_space;
    
   _Bool 
        readonly;
    
   _Bool 
        nonvolatile;
    
   _Bool 
        unmergeable;
};
typedef struct IOMMUTLBEntry IOMMUTLBEntry;
typedef enum {
    IOMMU_NONE = 0,
    IOMMU_RO = 1,
    IOMMU_WO = 2,
    IOMMU_RW = 3,
} IOMMUAccessFlags;
struct IOMMUTLBEntry {
    AddressSpace *target_as;
    hwaddr iova;
    hwaddr translated_addr;
    hwaddr addr_mask;
    IOMMUAccessFlags perm;
};
typedef enum {
    IOMMU_NOTIFIER_NONE = 0,
    IOMMU_NOTIFIER_UNMAP = 0x1,
    IOMMU_NOTIFIER_MAP = 0x2,
    IOMMU_NOTIFIER_DEVIOTLB_UNMAP = 0x04,
} IOMMUNotifierFlag;
struct IOMMUNotifier;
typedef void (*IOMMUNotify)(struct IOMMUNotifier *notifier,
                            IOMMUTLBEntry *data);
struct IOMMUNotifier {
    IOMMUNotify notify;
    IOMMUNotifierFlag notifier_flags;
    hwaddr start;
    hwaddr end;
    int iommu_idx;
    struct { struct IOMMUNotifier *le_next; struct IOMMUNotifier **le_prev; } node;
};
typedef struct IOMMUNotifier IOMMUNotifier;
typedef struct IOMMUTLBEvent {
    IOMMUNotifierFlag type;
    IOMMUTLBEntry entry;
} IOMMUTLBEvent;
struct MemoryRegionOps {
    uint64_t (*read)(void *opaque,
                     hwaddr addr,
                     unsigned size);
    void (*write)(void *opaque,
                  hwaddr addr,
                  uint64_t data,
                  unsigned size);
    MemTxResult (*read_with_attrs)(void *opaque,
                                   hwaddr addr,
                                   uint64_t *data,
                                   unsigned size,
                                   MemTxAttrs attrs);
    MemTxResult (*write_with_attrs)(void *opaque,
                                    hwaddr addr,
                                    uint64_t data,
                                    unsigned size,
                                    MemTxAttrs attrs);
    enum device_endian endianness;
    struct {
        unsigned min_access_size;
        unsigned max_access_size;
         
        _Bool 
             unaligned;
        
       _Bool 
            (*accepts)(void *opaque, hwaddr addr,
                        unsigned size, 
                                      _Bool 
                                           is_write,
                        MemTxAttrs attrs);
    } valid;
    struct {
        unsigned min_access_size;
        unsigned max_access_size;
        
       _Bool 
            unaligned;
    } impl;
};
typedef struct MemoryRegionClass {
    ObjectClass parent_class;
} MemoryRegionClass;
enum IOMMUMemoryRegionAttr {
    IOMMU_ATTR_SPAPR_TCE_FD
};
struct IOMMUMemoryRegionClass {
    MemoryRegionClass parent_class;
    IOMMUTLBEntry (*translate)(IOMMUMemoryRegion *iommu, hwaddr addr,
                               IOMMUAccessFlags flag, int iommu_idx);
    uint64_t (*get_min_page_size)(IOMMUMemoryRegion *iommu);
    int (*notify_flag_changed)(IOMMUMemoryRegion *iommu,
                               IOMMUNotifierFlag old_flags,
                               IOMMUNotifierFlag new_flags,
                               Error **errp);
    void (*replay)(IOMMUMemoryRegion *iommu, IOMMUNotifier *notifier);
    int (*get_attr)(IOMMUMemoryRegion *iommu, enum IOMMUMemoryRegionAttr attr,
                    void *data);
    int (*attrs_to_index)(IOMMUMemoryRegion *iommu, MemTxAttrs attrs);
    int (*num_indexes)(IOMMUMemoryRegion *iommu);
};
typedef struct RamDiscardListener RamDiscardListener;
typedef int (*NotifyRamPopulate)(RamDiscardListener *rdl,
                                 MemoryRegionSection *section);
typedef void (*NotifyRamDiscard)(RamDiscardListener *rdl,
                                 MemoryRegionSection *section);
struct RamDiscardListener {
    NotifyRamPopulate notify_populate;
    NotifyRamDiscard notify_discard;
    
   _Bool 
        double_discard_supported;
    MemoryRegionSection *section;
    struct { struct RamDiscardListener *le_next; struct RamDiscardListener **le_prev; } next;
};
typedef int (*ReplayRamPopulate)(MemoryRegionSection *section, void *opaque);
typedef void (*ReplayRamDiscard)(MemoryRegionSection *section, void *opaque);
struct RamDiscardManagerClass {
    InterfaceClass parent_class;
    uint64_t (*get_min_granularity)(const RamDiscardManager *rdm,
                                    const MemoryRegion *mr);
    
   _Bool 
        (*is_populated)(const RamDiscardManager *rdm,
                         const MemoryRegionSection *section);
    int (*replay_populated)(const RamDiscardManager *rdm,
                            MemoryRegionSection *section,
                            ReplayRamPopulate replay_fn, void *opaque);
    void (*replay_discarded)(const RamDiscardManager *rdm,
                             MemoryRegionSection *section,
                             ReplayRamDiscard replay_fn, void *opaque);
    void (*register_listener)(RamDiscardManager *rdm,
                              RamDiscardListener *rdl,
                              MemoryRegionSection *section);
    void (*unregister_listener)(RamDiscardManager *rdm,
                                RamDiscardListener *rdl);
};
uint64_t ram_discard_manager_get_min_granularity(const RamDiscardManager *rdm,
                                                 const MemoryRegion *mr);
_Bool 
    ram_discard_manager_is_populated(const RamDiscardManager *rdm,
                                      const MemoryRegionSection *section);
int ram_discard_manager_replay_populated(const RamDiscardManager *rdm,
                                         MemoryRegionSection *section,
                                         ReplayRamPopulate replay_fn,
                                         void *opaque);
void ram_discard_manager_replay_discarded(const RamDiscardManager *rdm,
                                          MemoryRegionSection *section,
                                          ReplayRamDiscard replay_fn,
                                          void *opaque);
void ram_discard_manager_register_listener(RamDiscardManager *rdm,
                                           RamDiscardListener *rdl,
                                           MemoryRegionSection *section);
void ram_discard_manager_unregister_listener(RamDiscardManager *rdm,
                                             RamDiscardListener *rdl);
_Bool 
    memory_get_xlat_addr(IOMMUTLBEntry *iotlb, void **vaddr,
                          ram_addr_t *ram_addr, 
                                               _Bool 
                                                    *read_only,
                          
                         _Bool 
                              *mr_has_discard_manager, Error **errp);
typedef struct CoalescedMemoryRange CoalescedMemoryRange;
typedef struct MemoryRegionIoeventfd MemoryRegionIoeventfd;
struct MemoryRegion {
    Object parent_obj;
    
   _Bool 
        romd_mode;
    
   _Bool 
        ram;
    
   _Bool 
        subpage;
    
   _Bool 
        readonly;
    
   _Bool 
        nonvolatile;
    
   _Bool 
        rom_device;
    
   _Bool 
        flush_coalesced_mmio;
    
   _Bool 
        unmergeable;
    uint8_t dirty_log_mask;
    
   _Bool 
        is_iommu;
    RAMBlock *ram_block;
    Object *owner;
    DeviceState *dev;
    const MemoryRegionOps *ops;
    void *opaque;
    MemoryRegion *container;
    int mapped_via_alias;
    Int128 size;
    hwaddr addr;
    void (*destructor)(MemoryRegion *mr);
    uint64_t align;
    
   _Bool 
        terminates;
    
   _Bool 
        ram_device;
    
   _Bool 
        enabled;
    uint8_t vga_logging_count;
    MemoryRegion *alias;
    hwaddr alias_offset;
    int32_t priority;
    union { struct MemoryRegion *tqh_first; QTailQLink tqh_circ; } subregions;
    union { struct MemoryRegion *tqe_next; QTailQLink tqe_circ; } subregions_link;
    union { struct CoalescedMemoryRange *tqh_first; QTailQLink tqh_circ; } coalesced;
    const char *name;
    unsigned ioeventfd_nb;
    MemoryRegionIoeventfd *ioeventfds;
    RamDiscardManager *rdm;
    
   _Bool 
        disable_reentrancy_guard;
};
struct IOMMUMemoryRegion {
    MemoryRegion parent_obj;
    struct { struct IOMMUNotifier *lh_first; } iommu_notify;
    IOMMUNotifierFlag iommu_notify_flags;
};
struct MemoryListener {
    void (*begin)(MemoryListener *listener);
    void (*commit)(MemoryListener *listener);
    void (*region_add)(MemoryListener *listener, MemoryRegionSection *section);
    void (*region_del)(MemoryListener *listener, MemoryRegionSection *section);
    void (*region_nop)(MemoryListener *listener, MemoryRegionSection *section);
    void (*log_start)(MemoryListener *listener, MemoryRegionSection *section,
                      int old_val, int new_val);
    void (*log_stop)(MemoryListener *listener, MemoryRegionSection *section,
                     int old_val, int new_val);
    void (*log_sync)(MemoryListener *listener, MemoryRegionSection *section);
    void (*log_sync_global)(MemoryListener *listener, 
                                                     _Bool 
                                                          last_stage);
    void (*log_clear)(MemoryListener *listener, MemoryRegionSection *section);
    
   _Bool 
        (*log_global_start)(MemoryListener *listener, Error **errp);
    void (*log_global_stop)(MemoryListener *listener);
    void (*log_global_after_sync)(MemoryListener *listener);
    void (*eventfd_add)(MemoryListener *listener, MemoryRegionSection *section,
                        
                       _Bool 
                            match_data, uint64_t data, EventNotifier *e);
    void (*eventfd_del)(MemoryListener *listener, MemoryRegionSection *section,
                        
                       _Bool 
                            match_data, uint64_t data, EventNotifier *e);
    void (*coalesced_io_add)(MemoryListener *listener, MemoryRegionSection *section,
                               hwaddr addr, hwaddr len);
    void (*coalesced_io_del)(MemoryListener *listener, MemoryRegionSection *section,
                               hwaddr addr, hwaddr len);
    unsigned priority;
    const char *name;
    AddressSpace *address_space;
    union { struct MemoryListener *tqe_next; QTailQLink tqe_circ; } link;
    union { struct MemoryListener *tqe_next; QTailQLink tqe_circ; } link_as;
};
typedef struct AddressSpaceMapClient {
    QEMUBH *bh;
    struct { struct AddressSpaceMapClient *le_next; struct AddressSpaceMapClient **le_prev; } link;
} AddressSpaceMapClient;
struct AddressSpace {
    struct rcu_head rcu;
    char *name;
    MemoryRegion *root;
    struct FlatView *current_map;
    int ioeventfd_nb;
    int ioeventfd_notifiers;
    struct MemoryRegionIoeventfd *ioeventfds;
    union { struct MemoryListener *tqh_first; QTailQLink tqh_circ; } listeners;
    union { struct AddressSpace *tqe_next; QTailQLink tqe_circ; } address_spaces_link;
    size_t max_bounce_buffer_size;
    size_t bounce_buffer_size;
    QemuMutex map_client_list_lock;
    struct { struct AddressSpaceMapClient *lh_first; } map_client_list;
};
typedef struct AddressSpaceDispatch AddressSpaceDispatch;
typedef struct FlatRange FlatRange;
struct FlatView {
    struct rcu_head rcu;
    unsigned ref;
    FlatRange *ranges;
    unsigned nr;
    unsigned nr_allocated;
    struct AddressSpaceDispatch *dispatch;
    MemoryRegion *root;
};
typedef 
       _Bool 
            (*flatview_cb)(Int128 start,
                            Int128 len,
                            const MemoryRegion *mr,
                            hwaddr offset_in_region,
                            void *opaque);
void flatview_for_each_range(FlatView *fv, flatview_cb cb, void *opaque);
MemoryRegionSection *memory_region_section_new_copy(MemoryRegionSection *s);
void memory_region_section_free_copy(MemoryRegionSection *s);
void memory_region_init(MemoryRegion *mr,
                        Object *owner,
                        const char *name,
                        uint64_t size);
void memory_region_ref(MemoryRegion *mr);
void memory_region_unref(MemoryRegion *mr);
void memory_region_init_io(MemoryRegion *mr,
                           Object *owner,
                           const MemoryRegionOps *ops,
                           void *opaque,
                           const char *name,
                           uint64_t size);
_Bool 
    memory_region_init_ram_nomigrate(MemoryRegion *mr,
                                      Object *owner,
                                      const char *name,
                                      uint64_t size,
                                      Error **errp);
_Bool 
    memory_region_init_ram_flags_nomigrate(MemoryRegion *mr,
                                            Object *owner,
                                            const char *name,
                                            uint64_t size,
                                            uint32_t ram_flags,
                                            Error **errp);
_Bool 
    memory_region_init_resizeable_ram(MemoryRegion *mr,
                                       Object *owner,
                                       const char *name,
                                       uint64_t size,
                                       uint64_t max_size,
                                       void (*resized)(const char*,
                                                       uint64_t length,
                                                       void *host),
                                       Error **errp);
_Bool 
    memory_region_init_ram_from_file(MemoryRegion *mr,
                                      Object *owner,
                                      const char *name,
                                      uint64_t size,
                                      uint64_t align,
                                      uint32_t ram_flags,
                                      const char *path,
                                      ram_addr_t offset,
                                      Error **errp);
_Bool 
    memory_region_init_ram_from_fd(MemoryRegion *mr,
                                    Object *owner,
                                    const char *name,
                                    uint64_t size,
                                    uint32_t ram_flags,
                                    int fd,
                                    ram_addr_t offset,
                                    Error **errp);
void memory_region_init_ram_ptr(MemoryRegion *mr,
                                Object *owner,
                                const char *name,
                                uint64_t size,
                                void *ptr);
void memory_region_init_ram_device_ptr(MemoryRegion *mr,
                                       Object *owner,
                                       const char *name,
                                       uint64_t size,
                                       void *ptr);
void memory_region_init_alias(MemoryRegion *mr,
                              Object *owner,
                              const char *name,
                              MemoryRegion *orig,
                              hwaddr offset,
                              uint64_t size);
_Bool 
    memory_region_init_rom_nomigrate(MemoryRegion *mr,
                                      Object *owner,
                                      const char *name,
                                      uint64_t size,
                                      Error **errp);
_Bool 
    memory_region_init_rom_device_nomigrate(MemoryRegion *mr,
                                             Object *owner,
                                             const MemoryRegionOps *ops,
                                             void *opaque,
                                             const char *name,
                                             uint64_t size,
                                             Error **errp);
void memory_region_init_iommu(void *_iommu_mr,
                              size_t instance_size,
                              const char *mrtypename,
                              Object *owner,
                              const char *name,
                              uint64_t size);
_Bool 
    memory_region_init_ram(MemoryRegion *mr,
                            Object *owner,
                            const char *name,
                            uint64_t size,
                            Error **errp);
_Bool 
    memory_region_init_ram_guest_memfd(MemoryRegion *mr,
                                        Object *owner,
                                        const char *name,
                                        uint64_t size,
                                        Error **errp);
_Bool 
    memory_region_init_rom(MemoryRegion *mr,
                            Object *owner,
                            const char *name,
                            uint64_t size,
                            Error **errp);
_Bool 
    memory_region_init_rom_device(MemoryRegion *mr,
                                   Object *owner,
                                   const MemoryRegionOps *ops,
                                   void *opaque,
                                   const char *name,
                                   uint64_t size,
                                   Error **errp);
Object *memory_region_owner(MemoryRegion *mr);
uint64_t memory_region_size(MemoryRegion *mr);
_Bool 
    memory_region_is_ram_device(MemoryRegion *mr);
_Bool 
    memory_region_is_protected(MemoryRegion *mr);
_Bool 
    memory_region_has_guest_memfd(MemoryRegion *mr);
uint64_t memory_region_iommu_get_min_page_size(IOMMUMemoryRegion *iommu_mr);
void memory_region_notify_iommu(IOMMUMemoryRegion *iommu_mr,
                                int iommu_idx,
                                const IOMMUTLBEvent event);
void memory_region_notify_iommu_one(IOMMUNotifier *notifier,
                                    const IOMMUTLBEvent *event);
void memory_region_unmap_iommu_notifier_range(IOMMUNotifier *notifier);
int memory_region_register_iommu_notifier(MemoryRegion *mr,
                                          IOMMUNotifier *n, Error **errp);
void memory_region_iommu_replay(IOMMUMemoryRegion *iommu_mr, IOMMUNotifier *n);
void memory_region_unregister_iommu_notifier(MemoryRegion *mr,
                                             IOMMUNotifier *n);
int memory_region_iommu_get_attr(IOMMUMemoryRegion *iommu_mr,
                                 enum IOMMUMemoryRegionAttr attr,
                                 void *data);
int memory_region_iommu_attrs_to_index(IOMMUMemoryRegion *iommu_mr,
                                       MemTxAttrs attrs);
int memory_region_iommu_num_indexes(IOMMUMemoryRegion *iommu_mr);
const char *memory_region_name(const MemoryRegion *mr);
_Bool 
    memory_region_is_logging(MemoryRegion *mr, uint8_t client);
uint8_t memory_region_get_dirty_log_mask(MemoryRegion *mr);
int memory_region_get_fd(MemoryRegion *mr);
MemoryRegion *memory_region_from_host(void *ptr, ram_addr_t *offset);
void *memory_region_get_ram_ptr(MemoryRegion *mr);
void memory_region_ram_resize(MemoryRegion *mr, ram_addr_t newsize,
                              Error **errp);
void memory_region_msync(MemoryRegion *mr, hwaddr addr, hwaddr size);
void memory_region_writeback(MemoryRegion *mr, hwaddr addr, hwaddr size);
void memory_region_set_log(MemoryRegion *mr, 
                                            _Bool 
                                                 log, unsigned client);
void memory_region_set_dirty(MemoryRegion *mr, hwaddr addr,
                             hwaddr size);
void memory_region_clear_dirty_bitmap(MemoryRegion *mr, hwaddr start,
                                      hwaddr len);
DirtyBitmapSnapshot *memory_region_snapshot_and_clear_dirty(MemoryRegion *mr,
                                                            hwaddr addr,
                                                            hwaddr size,
                                                            unsigned client);
_Bool 
    memory_region_snapshot_get_dirty(MemoryRegion *mr,
                                      DirtyBitmapSnapshot *snap,
                                      hwaddr addr, hwaddr size);
void memory_region_reset_dirty(MemoryRegion *mr, hwaddr addr,
                               hwaddr size, unsigned client);
void memory_region_flush_rom_device(MemoryRegion *mr, hwaddr addr, hwaddr size);
void memory_region_set_readonly(MemoryRegion *mr, 
                                                 _Bool 
                                                      readonly);
void memory_region_set_nonvolatile(MemoryRegion *mr, 
                                                    _Bool 
                                                         nonvolatile);
void memory_region_rom_device_set_romd(MemoryRegion *mr, 
                                                        _Bool 
                                                             romd_mode);
void memory_region_set_coalescing(MemoryRegion *mr);
void memory_region_add_coalescing(MemoryRegion *mr,
                                  hwaddr offset,
                                  uint64_t size);
void memory_region_clear_coalescing(MemoryRegion *mr);
void memory_region_set_flush_coalesced(MemoryRegion *mr);
void memory_region_clear_flush_coalesced(MemoryRegion *mr);
void memory_region_add_eventfd(MemoryRegion *mr,
                               hwaddr addr,
                               unsigned size,
                               
                              _Bool 
                                   match_data,
                               uint64_t data,
                               EventNotifier *e);
void memory_region_del_eventfd(MemoryRegion *mr,
                               hwaddr addr,
                               unsigned size,
                               
                              _Bool 
                                   match_data,
                               uint64_t data,
                               EventNotifier *e);
void memory_region_add_subregion(MemoryRegion *mr,
                                 hwaddr offset,
                                 MemoryRegion *subregion);
void memory_region_add_subregion_overlap(MemoryRegion *mr,
                                         hwaddr offset,
                                         MemoryRegion *subregion,
                                         int priority);
ram_addr_t memory_region_get_ram_addr(MemoryRegion *mr);
uint64_t memory_region_get_alignment(const MemoryRegion *mr);
void memory_region_del_subregion(MemoryRegion *mr,
                                 MemoryRegion *subregion);
void memory_region_set_enabled(MemoryRegion *mr, 
                                                _Bool 
                                                     enabled);
void memory_region_set_address(MemoryRegion *mr, hwaddr addr);
void memory_region_set_size(MemoryRegion *mr, uint64_t size);
void memory_region_set_alias_offset(MemoryRegion *mr,
                                    hwaddr offset);
void memory_region_set_unmergeable(MemoryRegion *mr, 
                                                    _Bool 
                                                         unmergeable);
_Bool 
    memory_region_present(MemoryRegion *container, hwaddr addr);
_Bool 
    memory_region_is_mapped(MemoryRegion *mr);
RamDiscardManager *memory_region_get_ram_discard_manager(MemoryRegion *mr);
void memory_region_set_ram_discard_manager(MemoryRegion *mr,
                                           RamDiscardManager *rdm);
MemoryRegionSection memory_region_find(MemoryRegion *mr,
                                       hwaddr addr, uint64_t size);
void memory_global_dirty_log_sync(
                                 _Bool 
                                      last_stage);
void memory_global_after_dirty_log_sync(void);
void memory_region_transaction_begin(void);
void memory_region_transaction_commit(void);
void memory_listener_register(MemoryListener *listener, AddressSpace *filter);
void memory_listener_unregister(MemoryListener *listener);
_Bool 
    memory_global_dirty_log_start(unsigned int flags, Error **errp);
void memory_global_dirty_log_stop(unsigned int flags);
void mtree_info(
               _Bool 
                    flatview, 
                              _Bool 
                                   dispatch_tree, 
                                                  _Bool 
                                                       owner, 
                                                              _Bool 
                                                                   disabled);
_Bool 
    memory_region_access_valid(MemoryRegion *mr, hwaddr addr,
                                unsigned size, 
                                              _Bool 
                                                   is_write,
                                MemTxAttrs attrs);
MemTxResult memory_region_dispatch_read(MemoryRegion *mr,
                                        hwaddr addr,
                                        uint64_t *pval,
                                        MemOp op,
                                        MemTxAttrs attrs);
MemTxResult memory_region_dispatch_write(MemoryRegion *mr,
                                         hwaddr addr,
                                         uint64_t data,
                                         MemOp op,
                                         MemTxAttrs attrs);
void address_space_init(AddressSpace *as, MemoryRegion *root, const char *name);
void address_space_destroy(AddressSpace *as);
void address_space_remove_listeners(AddressSpace *as);
MemTxResult address_space_rw(AddressSpace *as, hwaddr addr,
                             MemTxAttrs attrs, void *buf,
                             hwaddr len, 
                                        _Bool 
                                             is_write);
MemTxResult address_space_write(AddressSpace *as, hwaddr addr,
                                MemTxAttrs attrs,
                                const void *buf, hwaddr len);
MemTxResult address_space_write_rom(AddressSpace *as, hwaddr addr,
                                    MemTxAttrs attrs,
                                    const void *buf, hwaddr len);
uint8_t address_space_ldub(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint16_t address_space_lduw_le(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint16_t address_space_lduw_be(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl_le(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl_be(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq_le(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq_be(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
void address_space_stb(AddressSpace *as,
    hwaddr addr, uint8_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw_le(AddressSpace *as,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw_be(AddressSpace *as,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_le(AddressSpace *as,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_be(AddressSpace *as,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq_le(AddressSpace *as,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq_be(AddressSpace *as,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
struct MemoryRegionCache {
    uint8_t *ptr;
    hwaddr xlat;
    hwaddr len;
    FlatView *fv;
    MemoryRegionSection mrs;
    
   _Bool 
        is_write;
};
uint8_t address_space_ldub_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint16_t address_space_lduw_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint16_t address_space_lduw_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
void address_space_stb_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint8_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq_le_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq_be_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
int64_t address_space_cache_init(MemoryRegionCache *cache,
                                 AddressSpace *as,
                                 hwaddr addr,
                                 hwaddr len,
                                 
                                _Bool 
                                     is_write);
void address_space_cache_invalidate(MemoryRegionCache *cache,
                                    hwaddr addr,
                                    hwaddr access_len);
void address_space_cache_destroy(MemoryRegionCache *cache);
IOMMUTLBEntry address_space_get_iotlb_entry(AddressSpace *as, hwaddr addr,
                                            
                                           _Bool 
                                                is_write, MemTxAttrs attrs);
MemoryRegion *flatview_translate(FlatView *fv,
                                 hwaddr addr, hwaddr *xlat,
                                 hwaddr *len, 
                                             _Bool 
                                                  is_write,
                                 MemTxAttrs attrs);
_Bool 
    address_space_access_valid(AddressSpace *as, hwaddr addr, hwaddr len,
                                
                               _Bool 
                                    is_write, MemTxAttrs attrs);
void *address_space_map(AddressSpace *as, hwaddr addr,
                        hwaddr *plen, 
                                     _Bool 
                                          is_write, MemTxAttrs attrs);
void address_space_unmap(AddressSpace *as, void *buffer, hwaddr len,
                         
                        _Bool 
                             is_write, hwaddr access_len);
void address_space_register_map_client(AddressSpace *as, QEMUBH *bh);
void address_space_unregister_map_client(AddressSpace *as, QEMUBH *bh);
MemTxResult address_space_read_full(AddressSpace *as, hwaddr addr,
                                    MemTxAttrs attrs, void *buf, hwaddr len);
MemTxResult flatview_read_continue(FlatView *fv, hwaddr addr,
                                   MemTxAttrs attrs, void *buf,
                                   hwaddr len, hwaddr addr1, hwaddr l,
                                   MemoryRegion *mr);
void *qemu_map_ram_ptr(RAMBlock *ram_block, ram_addr_t addr);
MemTxResult address_space_read_cached_slow(MemoryRegionCache *cache,
                                           hwaddr addr, void *buf, hwaddr len);
MemTxResult address_space_write_cached_slow(MemoryRegionCache *cache,
                                            hwaddr addr, const void *buf,
                                            hwaddr len);
int memory_access_size(MemoryRegion *mr, unsigned l, hwaddr addr);
_Bool 
    prepare_mmio_access(MemoryRegion *mr);
MemTxResult address_space_set(AddressSpace *as, hwaddr addr,
                              uint8_t c, hwaddr len, MemTxAttrs attrs);
int ram_block_discard_disable(
                             _Bool 
                                  state);
int ram_block_uncoordinated_discard_disable(
                                           _Bool 
                                                state);
int ram_block_discard_require(
                             _Bool 
                                  state);
int ram_block_coordinated_discard_require(
                                         _Bool 
                                              state);
_Bool 
    ram_block_discard_is_disabled(void);
_Bool 
    ram_block_discard_is_required(void);
_Bool 
    target_words_bigendian(void);
uint16_t address_space_lduw(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq(AddressSpace *as,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_notdirty(AddressSpace *as,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw(AddressSpace *as,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl(AddressSpace *as,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq(AddressSpace *as,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
uint16_t address_space_lduw_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint32_t address_space_ldl_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
uint64_t address_space_ldq_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_notdirty_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stw_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint16_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stl_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint32_t val, MemTxAttrs attrs, MemTxResult *result);
void address_space_stq_cached_slow(MemoryRegionCache *cache,
    hwaddr addr, uint64_t val, MemTxAttrs attrs, MemTxResult *result);
CPUArchState *cpu_copy(CPUArchState *env);
_Static_assert(!(((1 << (12 - 1)) | (1 << (12 - 2)) | (1 << (12 - 3)) | (1 << (12 - 5)) | (1 << (12 - 4))) & ((1 << 0) | (1 << 1) | (1 << 2))), "not expecting: " "TLB_FLAGS_MASK & TLB_SLOW_FLAGS_MASK");
_Static_assert(!(
__builtin_offsetof (
ArchCPU
, 
parent_obj
) 
!= 0), "not expecting: " "offsetof(ArchCPU, parent_obj) != 0");
_Static_assert(!(
__builtin_offsetof (
ArchCPU
, 
env
) 
!= sizeof(CPUState)), "not expecting: " "offsetof(ArchCPU, env) != sizeof(CPUState)");
enum {
    PPC_NONE = 0x0000000000000000ULL,
    PPC_INSNS_BASE = 0x0000000000000001ULL,
    PPC_64B = 0x0000000000000020ULL,
    PPC_64BX = 0x0000000000000040ULL,
    PPC_64H = 0x0000000000000080ULL,
    PPC_WAIT = 0x0000000000000100ULL,
    PPC_MFTB = 0x0000000000000200ULL,
    PPC_ISEL = 0x0000000000000800ULL,
    PPC_POPCNTB = 0x0000000000001000ULL,
    PPC_STRING = 0x0000000000002000ULL,
    PPC_CILDST = 0x0000000000004000ULL,
    PPC_FLOAT = 0x0000000000010000ULL,
    PPC_FLOAT_EXT = 0x0000000000020000ULL,
    PPC_FLOAT_FSQRT = 0x0000000000040000ULL,
    PPC_FLOAT_FRES = 0x0000000000080000ULL,
    PPC_FLOAT_FRSQRTE = 0x0000000000100000ULL,
    PPC_FLOAT_FRSQRTES = 0x0000000000200000ULL,
    PPC_FLOAT_FSEL = 0x0000000000400000ULL,
    PPC_FLOAT_STFIWX = 0x0000000000800000ULL,
    PPC_ALTIVEC = 0x0000000001000000ULL,
    PPC_SPE = 0x0000000002000000ULL,
    PPC_SPE_SINGLE = 0x0000000004000000ULL,
    PPC_SPE_DOUBLE = 0x0000000008000000ULL,
    PPC_MEM_TLBIA = 0x0000000010000000ULL,
    PPC_MEM_TLBIE = 0x0000000020000000ULL,
    PPC_MEM_TLBSYNC = 0x0000000040000000ULL,
    PPC_MEM_SYNC = 0x0000000080000000ULL,
    PPC_MEM_EIEIO = 0x0000000100000000ULL,
    PPC_CACHE = 0x0000000200000000ULL,
    PPC_CACHE_ICBI = 0x0000000400000000ULL,
    PPC_CACHE_DCBZ = 0x0000000800000000ULL,
    PPC_CACHE_DCBA = 0x0000002000000000ULL,
    PPC_CACHE_LOCK = 0x0000004000000000ULL,
    PPC_EXTERN = 0x0000010000000000ULL,
    PPC_SEGMENT = 0x0000020000000000ULL,
    PPC_6xx_TLB = 0x0000040000000000ULL,
    PPC_40x_TLB = 0x0000100000000000ULL,
    PPC_SEGMENT_64B = 0x0000200000000000ULL,
    PPC_SLBI = 0x0000400000000000ULL,
    PPC_WRTEE = 0x0001000000000000ULL,
    PPC_40x_EXCP = 0x0002000000000000ULL,
    PPC_405_MAC = 0x0004000000000000ULL,
    PPC_440_SPEC = 0x0008000000000000ULL,
    PPC_BOOKE = 0x0010000000000000ULL,
    PPC_MFAPIDI = 0x0020000000000000ULL,
    PPC_TLBIVA = 0x0040000000000000ULL,
    PPC_TLBIVAX = 0x0080000000000000ULL,
    PPC_4xx_COMMON = 0x0100000000000000ULL,
    PPC_40x_ICBT = 0x0200000000000000ULL,
    PPC_RFMCI = 0x0400000000000000ULL,
    PPC_RFDI = 0x0800000000000000ULL,
    PPC_DCR = 0x1000000000000000ULL,
    PPC_DCRX = 0x2000000000000000ULL,
    PPC_POPCNTWD = 0x8000000000000000ULL,
    PPC2_BOOKE206 = 0x0000000000000001ULL,
    PPC2_VSX = 0x0000000000000002ULL,
    PPC2_DFP = 0x0000000000000004ULL,
    PPC2_PRCNTL = 0x0000000000000008ULL,
    PPC2_DBRX = 0x0000000000000010ULL,
    PPC2_ISA205 = 0x0000000000000020ULL,
    PPC2_VSX207 = 0x0000000000000040ULL,
    PPC2_PERM_ISA206 = 0x0000000000000080ULL,
    PPC2_DIVE_ISA206 = 0x0000000000000100ULL,
    PPC2_ATOMIC_ISA206 = 0x0000000000000200ULL,
    PPC2_FP_CVT_ISA206 = 0x0000000000000400ULL,
    PPC2_FP_TST_ISA206 = 0x0000000000000800ULL,
    PPC2_BCTAR_ISA207 = 0x0000000000001000ULL,
    PPC2_LSQ_ISA207 = 0x0000000000002000ULL,
    PPC2_ALTIVEC_207 = 0x0000000000004000ULL,
    PPC2_ISA207S = 0x0000000000008000ULL,
    PPC2_FP_CVT_S64 = 0x0000000000010000ULL,
    PPC2_TM = 0x0000000000020000ULL,
    PPC2_PM_ISA206 = 0x0000000000040000ULL,
    PPC2_ISA300 = 0x0000000000080000ULL,
    PPC2_ISA310 = 0x0000000000100000ULL,
    PPC2_MEM_LWSYNC = 0x0000000000200000ULL,
    PPC2_BCDA_ISA206 = 0x0000000000400000ULL,
};
enum {
    ACCESS_CODE = 0x10,
    ACCESS_INT = 0x20,
    ACCESS_FLOAT = 0x30,
    ACCESS_RES = 0x40,
    ACCESS_EXT = 0x50,
    ACCESS_CACHE = 0x60,
};
enum {
    PPC6xx_INPUT_HRESET = 0,
    PPC6xx_INPUT_SRESET = 1,
    PPC6xx_INPUT_CKSTP_IN = 2,
    PPC6xx_INPUT_MCP = 3,
    PPC6xx_INPUT_SMI = 4,
    PPC6xx_INPUT_INT = 5,
    PPC6xx_INPUT_TBEN = 6,
    PPC6xx_INPUT_WAKEUP = 7,
    PPC6xx_INPUT_NB,
};
enum {
    PPCBookE_INPUT_HRESET = 0,
    PPCBookE_INPUT_SRESET = 1,
    PPCBookE_INPUT_CKSTP_IN = 2,
    PPCBookE_INPUT_MCP = 3,
    PPCBookE_INPUT_SMI = 4,
    PPCBookE_INPUT_INT = 5,
    PPCBookE_INPUT_CINT = 6,
    PPCBookE_INPUT_NB,
};
enum {
    PPCE500_INPUT_RESET_CORE = 0,
    PPCE500_INPUT_MCK = 1,
    PPCE500_INPUT_CINT = 3,
    PPCE500_INPUT_INT = 4,
    PPCE500_INPUT_DEBUG = 6,
    PPCE500_INPUT_NB,
};
enum {
    PPC40x_INPUT_RESET_CORE = 0,
    PPC40x_INPUT_RESET_CHIP = 1,
    PPC40x_INPUT_RESET_SYS = 2,
    PPC40x_INPUT_CINT = 3,
    PPC40x_INPUT_INT = 4,
    PPC40x_INPUT_HALT = 5,
    PPC40x_INPUT_DEBUG = 6,
    PPC40x_INPUT_NB,
};
enum {
    PPCRCPU_INPUT_PORESET = 0,
    PPCRCPU_INPUT_HRESET = 1,
    PPCRCPU_INPUT_SRESET = 2,
    PPCRCPU_INPUT_IRQ0 = 3,
    PPCRCPU_INPUT_IRQ1 = 4,
    PPCRCPU_INPUT_IRQ2 = 5,
    PPCRCPU_INPUT_IRQ3 = 6,
    PPCRCPU_INPUT_IRQ4 = 7,
    PPCRCPU_INPUT_IRQ5 = 8,
    PPCRCPU_INPUT_IRQ6 = 9,
    PPCRCPU_INPUT_IRQ7 = 10,
    PPCRCPU_INPUT_NB,
};
enum {
    PPC_INTERRUPT_RESET = 0x00001,
    PPC_INTERRUPT_WAKEUP = 0x00002,
    PPC_INTERRUPT_MCK = 0x00004,
    PPC_INTERRUPT_EXT = 0x00008,
    PPC_INTERRUPT_SMI = 0x00010,
    PPC_INTERRUPT_CEXT = 0x00020,
    PPC_INTERRUPT_DEBUG = 0x00040,
    PPC_INTERRUPT_THERM = 0x00080,
    PPC_INTERRUPT_DECR = 0x00100,
    PPC_INTERRUPT_HDECR = 0x00200,
    PPC_INTERRUPT_PIT = 0x00400,
    PPC_INTERRUPT_FIT = 0x00800,
    PPC_INTERRUPT_WDT = 0x01000,
    PPC_INTERRUPT_CDOORBELL = 0x02000,
    PPC_INTERRUPT_DOORBELL = 0x04000,
    PPC_INTERRUPT_PERFM = 0x08000,
    PPC_INTERRUPT_HMI = 0x10000,
    PPC_INTERRUPT_HDOORBELL = 0x20000,
    PPC_INTERRUPT_HVIRT = 0x40000,
    PPC_INTERRUPT_EBB = 0x80000,
};
enum {
    PCR_COMPAT_2_05 = (0x8000000000000000ULL >> (62)),
    PCR_COMPAT_2_06 = (0x8000000000000000ULL >> (61)),
    PCR_COMPAT_2_07 = (0x8000000000000000ULL >> (60)),
    PCR_COMPAT_3_00 = (0x8000000000000000ULL >> (59)),
    PCR_COMPAT_3_10 = (0x8000000000000000ULL >> (58)),
    PCR_VEC_DIS = (0x8000000000000000ULL >> (0)),
    PCR_VSX_DIS = (0x8000000000000000ULL >> (1)),
    PCR_TM_DIS = (0x8000000000000000ULL >> (2)),
};
enum {
    HMER_MALFUNCTION_ALERT = (0x8000000000000000ULL >> (0)),
    HMER_PROC_RECV_DONE = (0x8000000000000000ULL >> (2)),
    HMER_PROC_RECV_ERROR_MASKED = (0x8000000000000000ULL >> (3)),
    HMER_TFAC_ERROR = (0x8000000000000000ULL >> (4)),
    HMER_TFMR_PARITY_ERROR = (0x8000000000000000ULL >> (5)),
    HMER_XSCOM_FAIL = (0x8000000000000000ULL >> (8)),
    HMER_XSCOM_DONE = (0x8000000000000000ULL >> (9)),
    HMER_PROC_RECV_AGAIN = (0x8000000000000000ULL >> (11)),
    HMER_WARN_RISE = (0x8000000000000000ULL >> (14)),
    HMER_WARN_FALL = (0x8000000000000000ULL >> (15)),
    HMER_SCOM_FIR_HMI = (0x8000000000000000ULL >> (16)),
    HMER_TRIG_FIR_HMI = (0x8000000000000000ULL >> (17)),
    HMER_HYP_RESOURCE_ERR = (0x8000000000000000ULL >> (20)),
    HMER_XSCOM_STATUS_MASK = (((0x8000000000000000ULL >> (21)) - (0x8000000000000000ULL >> (23))) | (0x8000000000000000ULL >> (21))),
};
enum {
    TFMR_CONTROL_MASK = (((0x8000000000000000ULL >> (0)) - (0x8000000000000000ULL >> (24))) | (0x8000000000000000ULL >> (0))),
    TFMR_MASK_HMI = (0x8000000000000000ULL >> (10)),
    TFMR_TB_ECLIPZ = (0x8000000000000000ULL >> (14)),
    TFMR_LOAD_TOD_MOD = (0x8000000000000000ULL >> (16)),
    TFMR_MOVE_CHIP_TOD_TO_TB = (0x8000000000000000ULL >> (18)),
    TFMR_CLEAR_TB_ERRORS = (0x8000000000000000ULL >> (24)),
    TFMR_STATUS_MASK = (((0x8000000000000000ULL >> (25)) - (0x8000000000000000ULL >> (63))) | (0x8000000000000000ULL >> (25))),
    TFMR_TBST_ENCODED = (((0x8000000000000000ULL >> (28)) - (0x8000000000000000ULL >> (31))) | (0x8000000000000000ULL >> (28))),
    TFMR_TBST_LAST = (((0x8000000000000000ULL >> (32)) - (0x8000000000000000ULL >> (35))) | (0x8000000000000000ULL >> (32))),
    TFMR_TB_ENABLED = (0x8000000000000000ULL >> (40)),
    TFMR_TB_VALID = (0x8000000000000000ULL >> (41)),
    TFMR_TB_SYNC_OCCURED = (0x8000000000000000ULL >> (42)),
    TFMR_FIRMWARE_CONTROL_ERROR = (0x8000000000000000ULL >> (46)),
};
enum {
    TBST_RESET = 0x0,
    TBST_SEND_TOD_MOD = 0x1,
    TBST_NOT_SET = 0x2,
    TBST_SYNC_WAIT = 0x6,
    TBST_GET_TOD = 0x7,
    TBST_TB_RUNNING = 0x8,
    TBST_TB_ERROR = 0x9,
};
target_ulong cpu_read_xer(const CPUPPCState *env);
void cpu_write_xer(CPUPPCState *env, target_ulong xer);
__attribute__ ((__noreturn__)) void raise_exception(CPUPPCState *env, uint32_t exception);
__attribute__ ((__noreturn__)) void raise_exception_ra(CPUPPCState *env, uint32_t exception,
                                   uintptr_t raddr);
__attribute__ ((__noreturn__)) void raise_exception_err(CPUPPCState *env, uint32_t exception,
                                    uint32_t error_code);
__attribute__ ((__noreturn__)) void raise_exception_err_ra(CPUPPCState *env, uint32_t exception,
                                       uint32_t error_code, uintptr_t raddr);
void dump_mmu(CPUPPCState *env);
void ppc_maybe_bswap_register(CPUPPCState *env, uint8_t *mem_buf, int len);
void ppc_store_vscr(CPUPPCState *env, uint32_t vscr);
uint32_t ppc_get_vscr(CPUPPCState *env);
void ppc_set_cr(CPUPPCState *env, uint64_t cr);
uint64_t ppc_get_cr(const CPUPPCState *env);
typedef target_ulong target_ptr_t;
typedef int32_t target_pid_t;
       
typedef enum panda_cb_type {
    PANDA_CB_BEFORE_BLOCK_TRANSLATE,
    PANDA_CB_AFTER_BLOCK_TRANSLATE,
    PANDA_CB_BLOCK_TRANSLATE,
    PANDA_CB_BEFORE_BLOCK_EXEC_INVALIDATE_OPT,
    PANDA_CB_BEFORE_TCG_CODEGEN,
    PANDA_CB_BEFORE_BLOCK_EXEC,
    PANDA_CB_AFTER_BLOCK_EXEC,
    PANDA_CB_INSN_TRANSLATE,
    PANDA_CB_INSN_EXEC,
    PANDA_CB_AFTER_INSN_TRANSLATE,
    PANDA_CB_AFTER_INSN_EXEC,
    PANDA_CB_VIRT_MEM_BEFORE_READ,
    PANDA_CB_VIRT_MEM_BEFORE_WRITE,
    PANDA_CB_PHYS_MEM_BEFORE_READ,
    PANDA_CB_PHYS_MEM_BEFORE_WRITE,
    PANDA_CB_VIRT_MEM_AFTER_READ,
    PANDA_CB_VIRT_MEM_AFTER_WRITE,
    PANDA_CB_PHYS_MEM_AFTER_READ,
    PANDA_CB_PHYS_MEM_AFTER_WRITE,
    PANDA_CB_MMIO_AFTER_READ,
    PANDA_CB_MMIO_BEFORE_WRITE,
    PANDA_CB_HD_READ,
    PANDA_CB_HD_WRITE,
    PANDA_CB_GUEST_HYPERCALL,
    PANDA_CB_MONITOR,
    PANDA_CB_QMP,
    PANDA_CB_CPU_RESTORE_STATE,
    PANDA_CB_BEFORE_LOADVM,
    PANDA_CB_ASID_CHANGED,
    PANDA_CB_REPLAY_HD_TRANSFER,
    PANDA_CB_REPLAY_NET_TRANSFER,
    PANDA_CB_REPLAY_SERIAL_RECEIVE,
    PANDA_CB_REPLAY_SERIAL_READ,
    PANDA_CB_REPLAY_SERIAL_SEND,
    PANDA_CB_REPLAY_SERIAL_WRITE,
    PANDA_CB_REPLAY_BEFORE_DMA,
    PANDA_CB_REPLAY_AFTER_DMA,
    PANDA_CB_REPLAY_HANDLE_PACKET,
    PANDA_CB_AFTER_CPU_EXEC_ENTER,
    PANDA_CB_BEFORE_CPU_EXEC_EXIT,
    PANDA_CB_AFTER_MACHINE_INIT,
    PANDA_CB_AFTER_LOADVM,
    PANDA_CB_TOP_LOOP,
    PANDA_CB_DURING_MACHINE_INIT,
    PANDA_CB_MAIN_LOOP_WAIT,
    PANDA_CB_PRE_SHUTDOWN,
    PANDA_CB_UNASSIGNED_IO_READ,
    PANDA_CB_UNASSIGNED_IO_WRITE,
    PANDA_CB_BEFORE_HANDLE_EXCEPTION,
    PANDA_CB_BEFORE_HANDLE_INTERRUPT,
    PANDA_CB_START_BLOCK_EXEC,
    PANDA_CB_END_BLOCK_EXEC,
    PANDA_CB_LAST
} panda_cb_type;
typedef union panda_cb {
    
   _Bool 
        (*before_block_exec_invalidate_opt)(CPUState *env, TranslationBlock *tb);
    void (*before_tcg_codegen)(CPUState *env, TranslationBlock *tb);
    void (*before_block_exec)(CPUState *env, TranslationBlock *tb);
    void (*after_block_exec)(CPUState *env, TranslationBlock *tb, uint8_t exitCode);
    void (*before_block_translate)(CPUState *env, uint64_t pc);
    void (*after_block_translate)(CPUState *env, TranslationBlock *tb);
    void (*block_translate)(CPUState *env, struct qemu_plugin_tb *tb);
    void (*after_cpu_exec_enter)(CPUState *env);
    void (*before_cpu_exec_exit)(CPUState *env, 
                                               _Bool 
                                                    ranBlock);
    
   _Bool 
        (*insn_translate)(CPUState *env, uint64_t pc);
    int (*insn_exec)(CPUState *env, uint64_t pc);
    
   _Bool 
        (*after_insn_translate)(CPUState *env, uint64_t pc);
    int (*after_insn_exec)(CPUState *env, uint64_t pc);
    void (*virt_mem_before_read)(CPUState *env, uint64_t pc, uint64_t addr, size_t size);
    void (*virt_mem_before_write)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_before_read)(CPUState *env, uint64_t pc, uint64_t addr, size_t size);
    void (*phys_mem_before_write)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*virt_mem_after_read)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*virt_mem_after_write)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_after_read)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_after_write)(CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*mmio_after_read)(CPUState *env, uint64_t physaddr, uint64_t vaddr, size_t size, uint64_t *val);
    void (*mmio_before_write)(CPUState *env, uint64_t physaddr, uint64_t vaddr, size_t size, uint64_t *val);
    void (*hd_read)(CPUState *env);
    void (*hd_write)(CPUState *env);
    
   _Bool 
        (*guest_hypercall)(CPUState *env);
    int (*monitor)(Monitor *mon, const char *cmd);
    
   _Bool 
        (*qmp)(char *command, char* args, char **result);
    void (*cpu_restore_state)(CPUState *env, TranslationBlock *tb);
    int (*before_loadvm)(void);
    
   _Bool 
        (*asid_changed)(CPUState *env, uint64_t oldval, uint64_t newval);
    void (*replay_hd_transfer)(CPUState *env, uint32_t type, uint64_t src_addr, uint64_t dest_addr, size_t num_bytes);
    void (*replay_before_dma)(CPUState *env, const uint8_t *buf, hwaddr addr, size_t size, 
                                                                                          _Bool 
                                                                                               is_write);
    void (*replay_after_dma)(CPUState *env, const uint8_t *buf, hwaddr addr, size_t size, 
                                                                                         _Bool 
                                                                                              is_write);
    void (*replay_handle_packet)(CPUState *env, uint8_t *buf, size_t size, uint8_t direction, uint64_t buf_addr_rec);
    void (*replay_net_transfer)(CPUState *env, uint32_t type, uint64_t src_addr, uint64_t dest_addr, size_t num_bytes);
    void (*replay_serial_receive)(CPUState *env, uint64_t fifo_addr, uint8_t value);
    void (*replay_serial_read)(CPUState *env, uint64_t fifo_addr, uint32_t port_addr, uint8_t value);
    void (*replay_serial_send)(CPUState *env, uint64_t fifo_addr, uint8_t value);
    void (*replay_serial_write)(CPUState *env, uint64_t fifo_addr, uint32_t port_addr, uint8_t value);
    void (*after_machine_init)(CPUState *env);
    void (*after_loadvm)(CPUState *env);
    void (*top_loop)(CPUState *env);
    void (*during_machine_init)(MachineState *machine);
    void (*main_loop_wait)(void);
    void (*pre_shutdown)(void);
    
   _Bool 
        (*unassigned_io_read)(CPUState *env, uint64_t pc, hwaddr addr, size_t size, uint64_t *val);
    
   _Bool 
        (*unassigned_io_write)(CPUState *env, uint64_t pc, hwaddr addr, size_t size, uint64_t val);
    int32_t (*before_handle_exception)(CPUState *cpu, int32_t exception_index);
    int32_t (*before_handle_interrupt)(CPUState *cpu, int32_t interrupt_request);
    void (*start_block_exec)(CPUState *cpu, TranslationBlock* tb);
    void (*end_block_exec)(CPUState *cpu, TranslationBlock* tb);
    void (*cbaddr)(void);
} panda_cb;
typedef union panda_cb_with_context {
    
   _Bool 
        (*before_block_exec_invalidate_opt)(void* context, CPUState *env, TranslationBlock *tb);
    void (*before_tcg_codegen)(void* context, CPUState *env, TranslationBlock *tb);
    void (*before_block_exec)(void* context, CPUState *env, TranslationBlock *tb);
    void (*after_block_exec)(void* context, CPUState *env, TranslationBlock *tb, uint8_t exitCode);
    void (*before_block_translate)(void* context, CPUState *env, uint64_t pc);
    void (*after_block_translate)(void* context, CPUState *env, TranslationBlock *tb);
    void (*block_translate)(void* context, CPUState *env, struct qemu_plugin_tb *tb);
    void (*after_cpu_exec_enter)(void* context, CPUState *env);
    void (*before_cpu_exec_exit)(void* context, CPUState *env, 
                                                              _Bool 
                                                                   ranBlock);
    
   _Bool 
        (*insn_translate)(void* context, CPUState *env, uint64_t pc);
    int (*insn_exec)(void* context, CPUState *env, uint64_t pc);
    
   _Bool 
        (*after_insn_translate)(void* context, CPUState *env, uint64_t pc);
    int (*after_insn_exec)(void* context, CPUState *env, uint64_t pc);
    void (*virt_mem_before_read)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size);
    void (*virt_mem_before_write)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_before_read)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size);
    void (*phys_mem_before_write)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*virt_mem_after_read)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*virt_mem_after_write)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_after_read)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*phys_mem_after_write)(void* context, CPUState *env, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf);
    void (*mmio_after_read)(void* context, CPUState *env, uint64_t physaddr, uint64_t vaddr, size_t size, uint64_t *val);
    void (*mmio_before_write)(void* context, CPUState *env, uint64_t physaddr, uint64_t vaddr, size_t size, uint64_t *val);
    void (*hd_read)(void* context, CPUState *env);
    void (*hd_write)(void* context, CPUState *env);
    
   _Bool 
        (*guest_hypercall)(void* context, CPUState *env);
    int (*monitor)(void* context, Monitor *mon, const char *cmd);
    
   _Bool 
        (*qmp)(void* context, char *command, char* args, char **result);
    void (*cpu_restore_state)(void* context, CPUState *env, TranslationBlock *tb);
    int (*before_loadvm)(void* context);
    
   _Bool 
        (*asid_changed)(void* context, CPUState *env, uint64_t oldval, uint64_t newval);
    void (*replay_hd_transfer)(void* context, CPUState *env, uint32_t type, uint64_t src_addr, uint64_t dest_addr, size_t num_bytes);
    void (*replay_before_dma)(void* context, CPUState *env, const uint8_t *buf, hwaddr addr, size_t size, 
                                                                                                         _Bool 
                                                                                                              is_write);
    void (*replay_after_dma)(void* context, CPUState *env, const uint8_t *buf, hwaddr addr, size_t size, 
                                                                                                        _Bool 
                                                                                                             is_write);
    void (*replay_handle_packet)(void* context, CPUState *env, uint8_t *buf, size_t size, uint8_t direction, uint64_t buf_addr_rec);
    void (*replay_net_transfer)(void* context, CPUState *env, uint32_t type, uint64_t src_addr, uint64_t dest_addr, size_t num_bytes);
    void (*replay_serial_receive)(void* context, CPUState *env, uint64_t fifo_addr, uint8_t value);
    void (*replay_serial_read)(void* context, CPUState *env, uint64_t fifo_addr, uint32_t port_addr, uint8_t value);
    void (*replay_serial_send)(void* context, CPUState *env, uint64_t fifo_addr, uint8_t value);
    void (*replay_serial_write)(void* context, CPUState *env, uint64_t fifo_addr, uint32_t port_addr, uint8_t value);
    void (*after_machine_init)(void* context, CPUState *env);
    void (*after_loadvm)(void* context, CPUState *env);
    void (*top_loop)(void* context, CPUState *env);
    void (*during_machine_init)(void* context, MachineState *machine);
    void (*main_loop_wait)(void* context);
    void (*pre_shutdown)(void* context);
    
   _Bool 
        (*unassigned_io_read)(void* context, CPUState *env, uint64_t pc, hwaddr addr, size_t size, uint64_t *val);
    
   _Bool 
        (*unassigned_io_write)(void* context, CPUState *env, uint64_t pc, hwaddr addr, size_t size, uint64_t val);
    int32_t (*before_handle_exception)(void* context, CPUState *cpu, int32_t exception_index);
    int32_t (*before_handle_interrupt)(void* context, CPUState *cpu, int32_t interrupt_request);
    void (*start_block_exec)(void* context, CPUState *cpu, TranslationBlock* tb);
    void (*end_block_exec)(void* context, CPUState *cpu, TranslationBlock* tb);
    void (*cbaddr)(void);
} panda_cb_with_context;
       
       
       
struct AccelState {
    Object parent_obj;
};
typedef struct AccelClass {
    ObjectClass parent_class;
    const char *name;
    int (*init_machine)(MachineState *ms);
    void (*setup_post)(MachineState *ms, AccelState *accel);
    
   _Bool 
        (*has_memory)(MachineState *ms, AddressSpace *as,
                       hwaddr start_addr, hwaddr size);
    
   _Bool 
        (*cpu_common_realize)(CPUState *cpu, Error **errp);
    void (*cpu_common_unrealize)(CPUState *cpu);
    int (*gdbstub_supported_sstep_flags)(void);
    
   _Bool 
        *allowed;
    GPtrArray *compat_props;
} AccelClass;
AccelClass *accel_find(const char *opt_name);
AccelState *current_accel(void);
const char *current_accel_name(void);
void accel_init_interfaces(AccelClass *ac);
int accel_init_machine(AccelState *accel, MachineState *ms);
void accel_setup_post(MachineState *ms);
void accel_cpu_instance_init(CPUState *cpu);
_Bool 
    accel_cpu_common_realize(CPUState *cpu, Error **errp);
void accel_cpu_common_unrealize(CPUState *cpu);
int accel_supported_gdbstub_sstep_flags(void);
MemoryRegion *get_system_memory(void);
MemoryRegion *get_system_io(void);
extern AddressSpace address_space_memory;
extern AddressSpace address_space_io;
typedef ram_addr_t tb_page_addr_t;
struct tb_tc {
    const void *ptr;
    size_t size;
};
struct TranslationBlock {
    vaddr pc;
    uint64_t cs_base;
    uint32_t flags;
    uint32_t cflags;
    uint16_t size;
    uint16_t icount;
    struct tb_tc tc;
    uintptr_t page_next[2];
    tb_page_addr_t page_addr[2];
    QemuSpin jmp_lock;
    uint16_t jmp_reset_offset[2];
    uint16_t jmp_insn_offset[2];
    uintptr_t jmp_target_addr[2];
    uintptr_t jmp_list_head;
    uintptr_t jmp_list_next[2];
    uintptr_t jmp_dest[2];
};
_Bool 
    tcg_cflags_has(CPUState *cpu, uint32_t flags);
void tcg_cflags_set(CPUState *cpu, uint32_t flags);
void tlb_init(CPUState *cpu);
void tlb_destroy(CPUState *cpu);
void tlb_flush_page(CPUState *cpu, vaddr addr);
void tlb_flush_page_all_cpus_synced(CPUState *src, vaddr addr);
void tlb_flush(CPUState *cpu);
void tlb_flush_all_cpus_synced(CPUState *src_cpu);
void tlb_flush_page_by_mmuidx(CPUState *cpu, vaddr addr,
                              uint16_t idxmap);
void tlb_flush_page_by_mmuidx_all_cpus_synced(CPUState *cpu, vaddr addr,
                                              uint16_t idxmap);
void tlb_flush_by_mmuidx(CPUState *cpu, uint16_t idxmap);
void tlb_flush_by_mmuidx_all_cpus_synced(CPUState *cpu, uint16_t idxmap);
void tlb_flush_page_bits_by_mmuidx(CPUState *cpu, vaddr addr,
                                   uint16_t idxmap, unsigned bits);
void tlb_flush_page_bits_by_mmuidx_all_cpus_synced
    (CPUState *cpu, vaddr addr, uint16_t idxmap, unsigned bits);
void tlb_flush_range_by_mmuidx(CPUState *cpu, vaddr addr,
                               vaddr len, uint16_t idxmap,
                               unsigned bits);
void tlb_flush_range_by_mmuidx_all_cpus_synced(CPUState *cpu,
                                               vaddr addr,
                                               vaddr len,
                                               uint16_t idxmap,
                                               unsigned bits);
void tlb_set_page_full(CPUState *cpu, int mmu_idx, vaddr addr,
                       CPUTLBEntryFull *full);
void tlb_set_page_with_attrs(CPUState *cpu, vaddr addr,
                             hwaddr paddr, MemTxAttrs attrs,
                             int prot, int mmu_idx, vaddr size);
void tlb_set_page(CPUState *cpu, vaddr addr,
                  hwaddr paddr, int prot,
                  int mmu_idx, vaddr size);
void *probe_access(CPUArchState *env, vaddr addr, int size,
                   MMUAccessType access_type, int mmu_idx, uintptr_t retaddr);
int probe_access_flags(CPUArchState *env, vaddr addr, int size,
                       MMUAccessType access_type, int mmu_idx,
                       
                      _Bool 
                           nonfault, void **phost, uintptr_t retaddr);
int probe_access_full(CPUArchState *env, vaddr addr, int size,
                      MMUAccessType access_type, int mmu_idx,
                      
                     _Bool 
                          nonfault, void **phost,
                      CPUTLBEntryFull **pfull, uintptr_t retaddr);
int probe_access_full_mmu(CPUArchState *env, vaddr addr, int size,
                          MMUAccessType access_type, int mmu_idx,
                          void **phost, CPUTLBEntryFull **pfull);
void tb_phys_invalidate(TranslationBlock *tb, tb_page_addr_t page_addr);
void tb_invalidate_phys_range(tb_page_addr_t start, tb_page_addr_t last);
void tb_set_jmp_target(TranslationBlock *tb, int n, uintptr_t addr);
struct MemoryRegionSection *iotlb_to_section(CPUState *cpu,
                                             hwaddr index, MemTxAttrs attrs);
tb_page_addr_t get_page_addr_code_hostp(CPUArchState *env, vaddr addr,
                                        void **hostp);
void tlb_reset_dirty(CPUState *cpu, ram_addr_t start1, ram_addr_t length);
void tlb_reset_dirty_range_all(ram_addr_t start, ram_addr_t length);
MemoryRegionSection *
address_space_translate_for_iotlb(CPUState *cpu, int asidx, hwaddr addr,
                                  hwaddr *xlat, hwaddr *plen,
                                  MemTxAttrs attrs, int *prot);
hwaddr memory_region_section_get_iotlb(CPUState *cpu,
                                       MemoryRegionSection *section);
enum {
    GDB_SIGNAL_0 = 0,
    GDB_SIGNAL_INT = 2,
    GDB_SIGNAL_QUIT = 3,
    GDB_SIGNAL_TRAP = 5,
    GDB_SIGNAL_ABRT = 6,
    GDB_SIGNAL_ALRM = 14,
    GDB_SIGNAL_STOP = 17,
    GDB_SIGNAL_IO = 23,
    GDB_SIGNAL_XCPU = 24,
    GDB_SIGNAL_UNKNOWN = 143
};
typedef struct GDBProcess {
    uint32_t pid;
    
   _Bool 
        attached;
    char *target_xml;
} GDBProcess;
enum RSState {
    RS_INACTIVE,
    RS_IDLE,
    RS_GETLINE,
    RS_GETLINE_ESC,
    RS_GETLINE_RLE,
    RS_CHKSUM1,
    RS_CHKSUM2,
};
typedef struct GDBState {
    
   _Bool 
        init;
    CPUState *c_cpu;
    CPUState *g_cpu;
    CPUState *query_cpu;
    enum RSState state;
    char line_buf[4096];
    int line_buf_index;
    int line_sum;
    int line_csum;
    GByteArray *last_packet;
    int signal;
    
   _Bool 
        multiprocess;
    GDBProcess *processes;
    int process_num;
    GString *str_buf;
    GByteArray *mem_buf;
    int sstep_flags;
    int supported_sstep_flags;
    
   _Bool 
        allow_stop_reply;
} GDBState;
extern GDBState gdbserver_state;
void gdb_put_strbuf(void);
int gdb_put_packet_binary(const char *buf, int len, 
                                                   _Bool 
                                                        dump);
void gdb_memtohex(GString *buf, const uint8_t *mem, int len);
void gdb_memtox(GString *buf, const char *mem, int len);
void gdb_read_byte(uint8_t ch);
_Bool 
    gdb_got_immediate_ack(void);
GDBProcess *gdb_get_process(uint32_t pid);
CPUState *gdb_get_first_cpu_in_process(GDBProcess *process);
CPUState *gdb_first_attached_cpu(void);
void gdb_append_thread_id(CPUState *cpu, GString *buf);
int gdb_get_cpu_index(CPUState *cpu);
unsigned int gdb_get_max_cpus(void);
_Bool 
    gdb_can_reverse(void);
int gdb_target_sigtrap(void);
void gdb_create_default_process(GDBState *s);
int gdb_signal_to_target(int sig);
int gdb_target_signal_to_gdb(int sig);
int gdb_get_char(void);
void gdb_continue(void);
int gdb_continue_partial(char *newstates);
void gdb_put_buffer(const uint8_t *buf, int len);
void gdb_init_gdbserver_state(void);
void gdb_handle_query_rcmd(GArray *params, void *ctx);
void gdb_handle_query_offsets(GArray *params, void *user_ctx);
void gdb_handle_query_xfer_auxv(GArray *params, void *user_ctx);
void gdb_handle_query_xfer_siginfo(GArray *params, void *user_ctx);
void gdb_handle_v_file_open(GArray *params, void *user_ctx);
void gdb_handle_v_file_close(GArray *params, void *user_ctx);
void gdb_handle_v_file_pread(GArray *params, void *user_ctx);
void gdb_handle_v_file_readlink(GArray *params, void *user_ctx);
void gdb_handle_query_xfer_exec_file(GArray *params, void *user_ctx);
void gdb_handle_set_catch_syscalls(GArray *params, void *user_ctx);
void gdb_handle_query_supported_user(const char *gdb_supported);
_Bool 
    gdb_handle_set_thread_user(uint32_t pid, uint32_t tid);
_Bool 
    gdb_handle_detach_user(uint32_t pid);
void gdb_handle_query_attached(GArray *params, void *ctx);
void gdb_handle_query_qemu_phy_mem_mode(GArray *params, void *ctx);
void gdb_handle_set_qemu_phy_mem_mode(GArray *params, void *ctx);
void gdb_handle_file_io(GArray *params, void *user_ctx);
_Bool 
    gdb_handled_syscall(void);
void gdb_disable_syscalls(void);
void gdb_syscall_reset(void);
void gdb_syscall_handling(const char *syscall_packet);
_Bool 
    gdb_supports_guest_debug(void);
int gdb_breakpoint_insert(CPUState *cs, int type, vaddr addr, vaddr len);
int gdb_breakpoint_remove(CPUState *cs, int type, vaddr addr, vaddr len);
void gdb_breakpoint_remove_all(CPUState *cs);
int gdb_target_memory_rw_debug(CPUState *cs, hwaddr addr,
                               uint8_t *buf, int len, 
                                                     _Bool 
                                                          is_write);
void panda_cleanup(void);
void panda_set_os_name(char *os_name);
void panda_before_find_fast(void);
void panda_disas(FILE *out, void *code, unsigned long size);
void panda_break_main_loop(void);
MemoryRegion* panda_find_ram(void);
extern 
      _Bool 
           panda_exit_loop;
extern 
      _Bool 
           panda_break_vl_loop_req;
target_ulong panda_current_asid(CPUState *env);
target_ulong panda_current_pc(CPUState *cpu);
Int128 panda_find_max_ram_address(void);
_Bool 
    enter_priv(CPUState* cpu);
void exit_priv(CPUState* cpu);
typedef enum {
    TCG_MO_LD_LD = 0x01,
    TCG_MO_ST_LD = 0x02,
    TCG_MO_LD_ST = 0x04,
    TCG_MO_ST_ST = 0x08,
    TCG_MO_ALL = 0x0F,
    TCG_BAR_LDAQ = 0x10,
    TCG_BAR_STRL = 0x20,
    TCG_BAR_SC = 0x30,
} TCGBar;
typedef enum {
    TCG_REG_EAX = 0,
    TCG_REG_ECX,
    TCG_REG_EDX,
    TCG_REG_EBX,
    TCG_REG_ESP,
    TCG_REG_EBP,
    TCG_REG_ESI,
    TCG_REG_EDI,
    TCG_REG_R8,
    TCG_REG_R9,
    TCG_REG_R10,
    TCG_REG_R11,
    TCG_REG_R12,
    TCG_REG_R13,
    TCG_REG_R14,
    TCG_REG_R15,
    TCG_REG_XMM0,
    TCG_REG_XMM1,
    TCG_REG_XMM2,
    TCG_REG_XMM3,
    TCG_REG_XMM4,
    TCG_REG_XMM5,
    TCG_REG_XMM6,
    TCG_REG_XMM7,
    TCG_REG_XMM8,
    TCG_REG_XMM9,
    TCG_REG_XMM10,
    TCG_REG_XMM11,
    TCG_REG_XMM12,
    TCG_REG_XMM13,
    TCG_REG_XMM14,
    TCG_REG_XMM15,
    TCG_REG_RAX = TCG_REG_EAX,
    TCG_REG_RCX = TCG_REG_ECX,
    TCG_REG_RDX = TCG_REG_EDX,
    TCG_REG_RBX = TCG_REG_EBX,
    TCG_REG_RSP = TCG_REG_ESP,
    TCG_REG_RBP = TCG_REG_EBP,
    TCG_REG_RSI = TCG_REG_ESI,
    TCG_REG_RDI = TCG_REG_EDI,
    TCG_AREG0 = TCG_REG_EBP,
    TCG_REG_CALL_STACK = TCG_REG_ESP
} TCGReg;
typedef enum {
    TCG_COND_NEVER = 0 | 0 | 0 | 0,
    TCG_COND_ALWAYS = 0 | 0 | 0 | 1,
    TCG_COND_EQ = 8 | 0 | 0 | 0,
    TCG_COND_NE = 8 | 0 | 0 | 1,
    TCG_COND_TSTEQ = 8 | 4 | 0 | 0,
    TCG_COND_TSTNE = 8 | 4 | 0 | 1,
    TCG_COND_LT = 0 | 0 | 2 | 0,
    TCG_COND_GE = 0 | 0 | 2 | 1,
    TCG_COND_GT = 0 | 4 | 2 | 0,
    TCG_COND_LE = 0 | 4 | 2 | 1,
    TCG_COND_LTU = 8 | 0 | 2 | 0,
    TCG_COND_GEU = 8 | 0 | 2 | 1,
    TCG_COND_GTU = 8 | 4 | 2 | 0,
    TCG_COND_LEU = 8 | 4 | 2 | 1,
} TCGCond;
typedef int64_t tcg_target_long;
typedef uint64_t tcg_target_ulong;
typedef uint32_t TCGRegSet;
typedef enum TCGOpcode {
INDEX_op_discard,
INDEX_op_set_label,
INDEX_op_call,
INDEX_op_br,
INDEX_op_mb,
INDEX_op_mov_i32,
INDEX_op_setcond_i32,
INDEX_op_negsetcond_i32,
INDEX_op_movcond_i32,
INDEX_op_ld8u_i32,
INDEX_op_ld8s_i32,
INDEX_op_ld16u_i32,
INDEX_op_ld16s_i32,
INDEX_op_ld_i32,
INDEX_op_st8_i32,
INDEX_op_st16_i32,
INDEX_op_st_i32,
INDEX_op_add_i32,
INDEX_op_sub_i32,
INDEX_op_mul_i32,
INDEX_op_div_i32,
INDEX_op_divu_i32,
INDEX_op_rem_i32,
INDEX_op_remu_i32,
INDEX_op_div2_i32,
INDEX_op_divu2_i32,
INDEX_op_and_i32,
INDEX_op_or_i32,
INDEX_op_xor_i32,
INDEX_op_shl_i32,
INDEX_op_shr_i32,
INDEX_op_sar_i32,
INDEX_op_rotl_i32,
INDEX_op_rotr_i32,
INDEX_op_deposit_i32,
INDEX_op_extract_i32,
INDEX_op_sextract_i32,
INDEX_op_extract2_i32,
INDEX_op_brcond_i32,
INDEX_op_add2_i32,
INDEX_op_sub2_i32,
INDEX_op_mulu2_i32,
INDEX_op_muls2_i32,
INDEX_op_muluh_i32,
INDEX_op_mulsh_i32,
INDEX_op_brcond2_i32,
INDEX_op_setcond2_i32,
INDEX_op_ext8s_i32,
INDEX_op_ext16s_i32,
INDEX_op_ext8u_i32,
INDEX_op_ext16u_i32,
INDEX_op_bswap16_i32,
INDEX_op_bswap32_i32,
INDEX_op_not_i32,
INDEX_op_neg_i32,
INDEX_op_andc_i32,
INDEX_op_orc_i32,
INDEX_op_eqv_i32,
INDEX_op_nand_i32,
INDEX_op_nor_i32,
INDEX_op_clz_i32,
INDEX_op_ctz_i32,
INDEX_op_ctpop_i32,
INDEX_op_mov_i64,
INDEX_op_setcond_i64,
INDEX_op_negsetcond_i64,
INDEX_op_movcond_i64,
INDEX_op_ld8u_i64,
INDEX_op_ld8s_i64,
INDEX_op_ld16u_i64,
INDEX_op_ld16s_i64,
INDEX_op_ld32u_i64,
INDEX_op_ld32s_i64,
INDEX_op_ld_i64,
INDEX_op_st8_i64,
INDEX_op_st16_i64,
INDEX_op_st32_i64,
INDEX_op_st_i64,
INDEX_op_add_i64,
INDEX_op_sub_i64,
INDEX_op_mul_i64,
INDEX_op_div_i64,
INDEX_op_divu_i64,
INDEX_op_rem_i64,
INDEX_op_remu_i64,
INDEX_op_div2_i64,
INDEX_op_divu2_i64,
INDEX_op_and_i64,
INDEX_op_or_i64,
INDEX_op_xor_i64,
INDEX_op_shl_i64,
INDEX_op_shr_i64,
INDEX_op_sar_i64,
INDEX_op_rotl_i64,
INDEX_op_rotr_i64,
INDEX_op_deposit_i64,
INDEX_op_extract_i64,
INDEX_op_sextract_i64,
INDEX_op_extract2_i64,
INDEX_op_ext_i32_i64,
INDEX_op_extu_i32_i64,
INDEX_op_extrl_i64_i32,
INDEX_op_extrh_i64_i32,
INDEX_op_brcond_i64,
INDEX_op_ext8s_i64,
INDEX_op_ext16s_i64,
INDEX_op_ext32s_i64,
INDEX_op_ext8u_i64,
INDEX_op_ext16u_i64,
INDEX_op_ext32u_i64,
INDEX_op_bswap16_i64,
INDEX_op_bswap32_i64,
INDEX_op_bswap64_i64,
INDEX_op_not_i64,
INDEX_op_neg_i64,
INDEX_op_andc_i64,
INDEX_op_orc_i64,
INDEX_op_eqv_i64,
INDEX_op_nand_i64,
INDEX_op_nor_i64,
INDEX_op_clz_i64,
INDEX_op_ctz_i64,
INDEX_op_ctpop_i64,
INDEX_op_add2_i64,
INDEX_op_sub2_i64,
INDEX_op_mulu2_i64,
INDEX_op_muls2_i64,
INDEX_op_muluh_i64,
INDEX_op_mulsh_i64,
INDEX_op_insn_start,
INDEX_op_exit_tb,
INDEX_op_goto_tb,
INDEX_op_goto_ptr,
INDEX_op_plugin_cb,
INDEX_op_plugin_mem_cb,
INDEX_op_qemu_ld_a32_i32,
INDEX_op_qemu_st_a32_i32,
INDEX_op_qemu_ld_a32_i64,
INDEX_op_qemu_st_a32_i64,
INDEX_op_qemu_ld_a64_i32,
INDEX_op_qemu_st_a64_i32,
INDEX_op_qemu_ld_a64_i64,
INDEX_op_qemu_st_a64_i64,
INDEX_op_qemu_st8_a32_i32,
INDEX_op_qemu_st8_a64_i32,
INDEX_op_qemu_ld_a32_i128,
INDEX_op_qemu_ld_a64_i128,
INDEX_op_qemu_st_a32_i128,
INDEX_op_qemu_st_a64_i128,
INDEX_op_mov_vec,
INDEX_op_dup_vec,
INDEX_op_dup2_vec,
INDEX_op_ld_vec,
INDEX_op_st_vec,
INDEX_op_dupm_vec,
INDEX_op_add_vec,
INDEX_op_sub_vec,
INDEX_op_mul_vec,
INDEX_op_neg_vec,
INDEX_op_abs_vec,
INDEX_op_ssadd_vec,
INDEX_op_usadd_vec,
INDEX_op_sssub_vec,
INDEX_op_ussub_vec,
INDEX_op_smin_vec,
INDEX_op_umin_vec,
INDEX_op_smax_vec,
INDEX_op_umax_vec,
INDEX_op_and_vec,
INDEX_op_or_vec,
INDEX_op_xor_vec,
INDEX_op_andc_vec,
INDEX_op_orc_vec,
INDEX_op_nand_vec,
INDEX_op_nor_vec,
INDEX_op_eqv_vec,
INDEX_op_not_vec,
INDEX_op_shli_vec,
INDEX_op_shri_vec,
INDEX_op_sari_vec,
INDEX_op_rotli_vec,
INDEX_op_shls_vec,
INDEX_op_shrs_vec,
INDEX_op_sars_vec,
INDEX_op_rotls_vec,
INDEX_op_shlv_vec,
INDEX_op_shrv_vec,
INDEX_op_sarv_vec,
INDEX_op_rotlv_vec,
INDEX_op_rotrv_vec,
INDEX_op_cmp_vec,
INDEX_op_bitsel_vec,
INDEX_op_cmpsel_vec,
INDEX_op_last_generic,
INDEX_op_x86_shufps_vec,
INDEX_op_x86_blend_vec,
INDEX_op_x86_packss_vec,
INDEX_op_x86_packus_vec,
INDEX_op_x86_psrldq_vec,
INDEX_op_x86_vperm2i128_vec,
INDEX_op_x86_punpckl_vec,
INDEX_op_x86_punpckh_vec,
INDEX_op_x86_vpshldi_vec,
INDEX_op_x86_vpshldv_vec,
INDEX_op_x86_vpshrdv_vec,
    NB_OPS,
} TCGOpcode;
typedef uint8_t tcg_insn_unit;
typedef struct TCGRelocation TCGRelocation;
struct TCGRelocation {
    struct { struct TCGRelocation *sqe_next; } next;
    tcg_insn_unit *ptr;
    intptr_t addend;
    int type;
};
typedef struct TCGOp TCGOp;
typedef struct TCGLabelUse TCGLabelUse;
struct TCGLabelUse {
    struct { struct TCGLabelUse *sqe_next; } next;
    TCGOp *op;
};
typedef struct TCGLabel TCGLabel;
struct TCGLabel {
    
   _Bool 
        present;
    
   _Bool 
        has_value;
    uint16_t id;
    union {
        uintptr_t value;
        const tcg_insn_unit *value_ptr;
    } u;
    struct { struct TCGLabelUse *sqh_first; struct TCGLabelUse **sqh_last; } branches;
    struct { struct TCGRelocation *sqh_first; struct TCGRelocation **sqh_last; } relocs;
    struct { struct TCGLabel *sqe_next; } next;
};
typedef struct TCGPool {
    struct TCGPool *next;
    int size;
    uint8_t data[] __attribute__ ((aligned));
} TCGPool;
typedef enum TCGType {
    TCG_TYPE_I32,
    TCG_TYPE_I64,
    TCG_TYPE_I128,
    TCG_TYPE_V64,
    TCG_TYPE_V128,
    TCG_TYPE_V256,
    TCG_TYPE_REG = TCG_TYPE_I64,
    TCG_TYPE_PTR = TCG_TYPE_I64,
} TCGType;
typedef tcg_target_ulong TCGArg;
typedef struct TCGv_i32_d *TCGv_i32;
typedef struct TCGv_i64_d *TCGv_i64;
typedef struct TCGv_i128_d *TCGv_i128;
typedef struct TCGv_ptr_d *TCGv_ptr;
typedef struct TCGv_vec_d *TCGv_vec;
typedef TCGv_ptr TCGv_env;
enum {
    TCG_BSWAP_IZ = 1,
    TCG_BSWAP_OZ = 2,
    TCG_BSWAP_OS = 4,
};
typedef enum TCGTempVal {
    TEMP_VAL_DEAD,
    TEMP_VAL_REG,
    TEMP_VAL_MEM,
    TEMP_VAL_CONST,
} TCGTempVal;
typedef enum TCGTempKind {
    TEMP_EBB,
    TEMP_TB,
    TEMP_GLOBAL,
    TEMP_FIXED,
    TEMP_CONST,
} TCGTempKind;
typedef struct TCGTemp {
    TCGReg reg:8;
    TCGTempVal val_type:8;
    TCGType base_type:8;
    TCGType type:8;
    TCGTempKind kind:3;
    unsigned int indirect_reg:1;
    unsigned int indirect_base:1;
    unsigned int mem_coherent:1;
    unsigned int mem_allocated:1;
    unsigned int temp_allocated:1;
    unsigned int temp_subindex:2;
    int64_t val;
    struct TCGTemp *mem_base;
    intptr_t mem_offset;
    const char *name;
    uintptr_t state;
    void *state_ptr;
} TCGTemp;
typedef struct TCGContext TCGContext;
typedef struct TCGTempSet {
    unsigned long l[(((512) + (8 * sizeof(long)) - 1) / (8 * sizeof(long)))];
} TCGTempSet;
typedef uint32_t TCGLifeData;
struct TCGOp {
    TCGOpcode opc : 8;
    unsigned nargs : 8;
    unsigned param1 : 8;
    unsigned param2 : 8;
    TCGLifeData life;
    union { struct TCGOp *tqe_next; QTailQLink tqe_circ; } link;
    TCGRegSet output_pref[2];
    TCGArg args[];
};
_Static_assert(!(NB_OPS > (1 << 8)), "not expecting: " "NB_OPS > (1 << 8)");
struct TCGContext {
    uint8_t *pool_cur, *pool_end;
    TCGPool *pool_first, *pool_current, *pool_first_large;
    int nb_labels;
    int nb_globals;
    int nb_temps;
    int nb_indirects;
    int nb_ops;
    TCGType addr_type;
    int page_mask;
    uint8_t page_bits;
    uint8_t tlb_dyn_max_bits;
    uint8_t insn_start_words;
    TCGBar guest_mo;
    TCGRegSet reserved_regs;
    intptr_t current_frame_offset;
    intptr_t frame_start;
    intptr_t frame_end;
    TCGTemp *frame_temp;
    TranslationBlock *gen_tb;
    tcg_insn_unit *code_buf;
    tcg_insn_unit *code_ptr;
    void *code_gen_buffer;
    size_t code_gen_buffer_size;
    void *code_gen_ptr;
    void *data_gen_ptr;
    void *code_gen_highwater;
    CPUState *cpu;
    struct { struct TCGLabelQemuLdst *sqh_first; struct TCGLabelQemuLdst **sqh_last; } ldst_labels;
    struct TCGLabelPoolData *pool_labels;
    TCGLabel *exitreq_label;
    struct qemu_plugin_tb *plugin_tb;
    const struct DisasContextBase *plugin_db;
    struct qemu_plugin_insn *plugin_insn;
    GHashTable *const_table[(TCG_TYPE_V256 + 1)];
    TCGTempSet free_temps[(TCG_TYPE_V256 + 1)];
    TCGTemp temps[512];
    union { struct TCGOp *tqh_first; QTailQLink tqh_circ; } ops, free_ops;
    struct { struct TCGLabel *sqh_first; struct TCGLabel **sqh_last; } labels;
    TCGOp *emit_before_op;
    TCGTemp *reg_to_temp[32];
    uint16_t gen_insn_end_off[512];
    uint64_t *gen_insn_data;
    sigjmp_buf jmp_trans;
};
extern __thread TCGContext *tcg_ctx;
extern const void *tcg_code_gen_epilogue;
extern uintptr_t tcg_splitwx_diff;
extern TCGv_env tcg_env;
_Bool 
    in_code_gen_buffer(const void *p);
void *tcg_malloc_internal(TCGContext *s, int size);
void tcg_pool_reset(TCGContext *s);
TranslationBlock *tcg_tb_alloc(TCGContext *s);
void tcg_region_reset_all(void);
size_t tcg_code_size(void);
size_t tcg_code_capacity(void);
void tcg_tb_insert(TranslationBlock *tb);
void tcg_tb_remove(TranslationBlock *tb);
TranslationBlock *tcg_tb_lookup(uintptr_t tc_ptr);
void tcg_tb_foreach(GTraverseFunc func, gpointer user_data);
size_t tcg_nb_tbs(void);
void tcg_func_start(TCGContext *s);
int tcg_gen_code(TCGContext *s, TranslationBlock *tb, uint64_t pc_start);
void tb_target_set_jmp_target(const TranslationBlock *, int,
                              uintptr_t, uintptr_t);
void tcg_set_frame(TCGContext *s, TCGReg reg, intptr_t start, intptr_t size);
typedef struct TCGArgConstraint {
    unsigned ct : 16;
    unsigned alias_index : 4;
    unsigned sort_index : 4;
    unsigned pair_index : 4;
    unsigned pair : 2;
    
   _Bool 
        oalias : 1;
    
   _Bool 
        ialias : 1;
    
   _Bool 
        newreg : 1;
    TCGRegSet regs;
} TCGArgConstraint;
enum {
    TCG_OPF_BB_EXIT = 0x01,
    TCG_OPF_BB_END = 0x02,
    TCG_OPF_CALL_CLOBBER = 0x04,
    TCG_OPF_SIDE_EFFECTS = 0x08,
    TCG_OPF_NOT_PRESENT = 0x20,
    TCG_OPF_VECTOR = 0x40,
    TCG_OPF_COND_BRANCH = 0x80
};
typedef struct TCGOpDef {
    const char *name;
    uint8_t nb_oargs, nb_iargs, nb_cargs, nb_args;
    uint8_t flags;
} TCGOpDef;
extern const TCGOpDef tcg_op_defs[];
extern const size_t tcg_op_defs_max;
_Bool 
    tcg_op_supported(TCGOpcode op, TCGType type, unsigned flags);
_Bool 
    tcg_op_deposit_valid(TCGType type, unsigned ofs, unsigned len);
void tcg_gen_call0(void *func, TCGHelperInfo *, TCGTemp *ret);
void tcg_gen_call1(void *func, TCGHelperInfo *, TCGTemp *ret, TCGTemp *);
void tcg_gen_call2(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *);
void tcg_gen_call3(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *);
void tcg_gen_call4(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *);
void tcg_gen_call5(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *);
void tcg_gen_call6(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *,
                   TCGTemp *, TCGTemp *);
void tcg_gen_call7(void *func, TCGHelperInfo *, TCGTemp *ret,
                   TCGTemp *, TCGTemp *, TCGTemp *, TCGTemp *,
                   TCGTemp *, TCGTemp *, TCGTemp *);
TCGOp *tcg_emit_op(TCGOpcode opc, unsigned nargs);
void tcg_op_remove(TCGContext *s, TCGOp *op);
void tcg_remove_ops_after(TCGOp *op);
void tcg_optimize(TCGContext *s);
TCGLabel *gen_new_label(void);
typedef uintptr_t tcg_prologue_fn(CPUArchState *env, const void *tb_ptr);
extern tcg_prologue_fn *tcg_qemu_tb_exec;
void tcg_register_jit(const void *buf, size_t buf_size);
int tcg_can_emit_vec_op(TCGOpcode, TCGType, unsigned);
void tcg_expand_vec_op(TCGOpcode, TCGType, unsigned, TCGArg, ...);
uint64_t dup_const(unsigned vece, uint64_t c);
_Bool 
    tcg_can_emit_vecop_list(const TCGOpcode *, TCGType, unsigned);
void tcg_dump_ops(TCGContext *s, FILE *f, 
                                         _Bool 
                                              have_prefs);
void disas(FILE *out, const void *code, size_t size);
void target_disas(FILE *out, CPUState *cpu, const DisasContextBase *db);
void monitor_disas(Monitor *mon, CPUState *cpu, uint64_t pc,
                   int nb_insn, 
                               _Bool 
                                    is_physical);
char *plugin_disas(CPUState *cpu, const DisasContextBase *db,
                   uint64_t addr, size_t size);
const char *lookup_symbol(uint64_t orig_addr);
struct syminfo;
struct elf32_sym;
struct elf64_sym;
typedef const char *(*lookup_symbol_t)(struct syminfo *s, uint64_t orig_addr);
struct syminfo {
    lookup_symbol_t lookup_symbol;
    unsigned int disas_num_syms;
    union {
      struct elf32_sym *elf32;
      struct elf64_sym *elf64;
    } disas_symtab;
    const char *disas_strtab;
    struct syminfo *next;
};
extern struct syminfo *syminfos;
typedef struct _panda_cb_list panda_cb_list;
struct _panda_cb_list {
    panda_cb_with_context entry;
    void *owner;
    panda_cb_list *next;
    panda_cb_list *prev;
    
   _Bool 
        enabled;
    void* context;
};
panda_cb_list *panda_cb_list_next(panda_cb_list *plist);
void panda_enable_plugin(void *plugin);
void panda_disable_plugin(void *plugin);
typedef struct panda_plugin {
    char *name;
    void *plugin;
    
   _Bool 
        unload;
    
   _Bool 
        exported_symbols;
} panda_plugin;
panda_cb_with_context panda_get_cb_trampoline(panda_cb_type type);
void panda_register_callback(void *plugin, panda_cb_type type, panda_cb cb);
void panda_register_callback_with_context(void *plugin, panda_cb_type type, panda_cb_with_context cb, void* context);
void panda_disable_callback(void *plugin, panda_cb_type type, panda_cb cb);
void panda_disable_callback_with_context(void *plugin, panda_cb_type type, panda_cb_with_context cb, void* context);
void panda_enable_callback(void *plugin, panda_cb_type type, panda_cb cb);
void panda_enable_callback_with_context(void *plugin, panda_cb_type type, panda_cb_with_context cb, void* context);
void panda_unregister_callbacks(void *plugin);
_Bool 
    panda_load_plugin(const char *filename, const char *plugin_name);
_Bool 
    panda_add_arg(const char *plugin_name, const char *plugin_arg);
void * panda_get_plugin_by_name(const char *name);
void panda_unload_plugin_by_name(const char* name);
void panda_do_unload_plugin(int index);
void panda_unload_plugin(void *plugin);
void panda_unload_plugin_idx(int idx);
void panda_unload_plugins(void);
extern 
      _Bool 
           panda_update_pc;
extern 
      _Bool 
           panda_use_memcb;
extern panda_cb_list *panda_cbs[PANDA_CB_LAST];
extern 
      _Bool 
           panda_tb_chaining;
typedef enum OSFamilyEnum { OS_UNKNOWN, OS_WINDOWS, OS_LINUX, OS_FREEBSD } PandaOsFamily;
extern char *panda_os_name;
extern char *panda_os_family;
extern char *panda_os_variant;
extern uint32_t panda_os_bits;
extern PandaOsFamily panda_os_familyno;
_Bool 
    panda_break_exec(void);
_Bool 
    panda_flush_tb(void);
void panda_do_flush_tb(void);
void panda_do_break_exec(void);
void panda_enable_precise_pc(void);
void panda_disable_precise_pc(void);
void panda_enable_memcb(void);
void panda_disable_memcb(void);
void panda_enable_llvm(void);
void panda_enable_llvm_no_exec(void);
void panda_disable_llvm(void);
void panda_enable_llvm_helpers(void);
void panda_disable_llvm_helpers(void);
int panda_write_current_llvm_bitcode_to_file(const char* path);
uintptr_t panda_get_current_llvm_module(void);
void panda_disable_tb_chaining(void);
void panda_enable_tb_chaining(void);
void panda_memsavep(FILE *file);
int panda_vm_quit(void);
char* panda_get_rr_name(void);
typedef struct panda_arg {
    char *argptr;
    char *key;
    char *value;
} panda_arg;
typedef struct panda_arg_list {
    int nargs;
    panda_arg *list;
    char *plugin_name;
} panda_arg_list;
panda_arg_list *panda_get_args(const char *plugin_name);
void panda_free_args(panda_arg_list *args);
target_ulong panda_parse_ulong(panda_arg_list *args, const char *argname, target_ulong defval);
target_ulong panda_parse_ulong_req(panda_arg_list *args, const char *argname, const char *help);
target_ulong panda_parse_ulong_opt(panda_arg_list *args, const char *argname, target_ulong defval, const char *help);
uint32_t panda_parse_uint32(panda_arg_list *args, const char *argname, uint32_t defval);
uint32_t panda_parse_uint32_req(panda_arg_list *args, const char *argname, const char *help);
uint32_t panda_parse_uint32_opt(panda_arg_list *args, const char *argname, uint32_t defval, const char *help);
uint64_t panda_parse_uint64(panda_arg_list *args, const char *argname, uint64_t defval);
uint64_t panda_parse_uint64_req(panda_arg_list *args, const char *argname, const char *help);
uint64_t panda_parse_uint64_opt(panda_arg_list *args, const char *argname, uint64_t defval, const char *help);
double panda_parse_double(panda_arg_list *args, const char *argname, double defval);
double panda_parse_double_req(panda_arg_list *args, const char *argname, const char *help);
double panda_parse_double_opt(panda_arg_list *args, const char *argname, double defval, const char *help);
_Bool 
    panda_parse_bool(panda_arg_list *args, const char *argname);
_Bool 
    panda_parse_bool_req(panda_arg_list *args, const char *argname, const char *help);
_Bool 
    panda_parse_bool_opt(panda_arg_list *args, const char *argname, const char *help);
const char *panda_parse_string(panda_arg_list *args, const char *argname, const char *defval);
const char *panda_parse_string_req(panda_arg_list *args, const char *argname, const char *help);
const char *panda_parse_string_opt(panda_arg_list *args, const char *argname, const char *defval, const char *help);
extern gchar *panda_argv[32];
extern int panda_argc;
char** str_split(char *a_str, const char a_delim);
char* resolve_file_from_plugin_directory(const char* file_name_fmt, const char* name);
char *panda_plugin_path(const char *name);
char* panda_shared_library_path(const char* name);
void panda_require(const char *name);
void panda_require_from_library(const char *plugin_name, char **plugin_args, uint32_t num_args);
_Bool 
    panda_is_callback_enabled(void *plugin, panda_cb_type type, panda_cb cb);
int panda_init(int argc, char **argv, char **envp);
int panda_run(void);
void panda_stop(int code);
void panda_cont(void);
void _panda_set_library_mode(const 
                                  _Bool
                                      );
void panda_start_pandalog(const char *name);
int panda_snap(char *name);
int panda_delvm(char *name);
int panda_revert(char *name);
void panda_reset(void);
int panda_finish(void);
_Bool 
    panda_was_aborted(void);
void panda_set_qemu_path(char* filepath);
void panda_set_extra_plugin_path(char *filepath);
int panda_init_plugin(char *plugin_name, char **plugin_args, uint32_t num_args);
void panda_register_callback_helper(void* plugin, panda_cb_type type, panda_cb* cb);
void panda_enable_callback_helper(void *plugin, panda_cb_type type, panda_cb* cb);
void panda_disable_callback_helper(void *plugin, panda_cb_type type, panda_cb* cb);
uint64_t rr_get_guest_instr_count_external(void);
int panda_virtual_memory_read_external(CPUState *cpu, target_ulong addr, char *buf, int len);
int panda_virtual_memory_write_external(CPUState *cpu, target_ulong addr, char *buf, int len);
int panda_physical_memory_read_external(hwaddr addr, uint8_t *buf, int len);
int panda_physical_memory_write_external(hwaddr addr, uint8_t *buf, int len);
target_ulong panda_get_retval_external(const CPUState *cpu);
MemTxResult PandaPhysicalAddressToRamOffset_external(ram_addr_t* out, hwaddr addr, 
                                                                                  _Bool 
                                                                                       is_write);
_Bool 
    panda_in_kernel_external(const CPUState *cpu);
_Bool 
    panda_in_kernel_mode_external(const CPUState *cpu);
_Bool 
    panda_in_kernel_code_linux_external(CPUState *cpu);
target_ulong panda_current_ksp_external(CPUState *cpu);
target_ulong panda_current_sp_external(const CPUState *cpu);
target_ulong panda_current_sp_masked_pagesize_external(const CPUState *cpu, target_ulong pagesize);
target_ulong panda_virt_to_phys_external(CPUState *cpu, target_ulong addr);
void panda_setup_signal_handling(void (*sigfun) (int, void*, void *));
void map_memory(char* name, uint64_t size, uint64_t address);
void panda_init_monitor(void);
char* panda_monitor_run(char* buf);
CPUState* get_cpu(void);
unsigned long garray_len(GArray *list);
CPUArchState *panda_cpu_env(CPUState *cpu);
void panda_cleanup_record(void);
//void (*panda_external_signal_handler)(int, siginfo_t*,void*);
CPUState *panda_current_cpu(int index);
CPUState *panda_cpu_in_translate(void);
TranslationBlock *panda_get_tb(struct qemu_plugin_tb *tb);
int panda_get_memcb_status(void);