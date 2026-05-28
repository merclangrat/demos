/*
 * Конфигурация мультиплексоров DHU
 * NDHU and NDM are in units of boards (16 lines each). LOWDM is the
 * unit number of the first unit with a DM-11 (e.g. 16 if the first
 * DH has no DM, the second does have one). All units from LOWDM through
 * LOWDM + (NDM*16) are assumed to have modem control (bit 0200 must be
 * on in * their minor device numbers if DH_SOFTCAR is defined).
 */
#define NDHU            0
#define NDM             0
#define LOWDM           0
#define DH_SOFTCAR
#define DHU_IOCTL
/*#define DHV11 /* only 8 lines */
