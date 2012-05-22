/*
 * machine_kexec.c - handle transition of Linux booting another kernel
 */

#include <linux/mm.h>
#include <linux/kexec.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/io.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/mmu_context.h>
#include <asm/cacheflush.h>
#include <asm/mach-types.h>

extern const unsigned char relocate_new_kernel[];
extern const unsigned int relocate_new_kernel_size;

extern void setup_mm_for_reboot(char mode);

extern unsigned long kexec_start_address;
extern unsigned long kexec_indirection_page;
extern unsigned long kexec_mach_type;
extern unsigned long kexec_boot_atags;

#ifdef CONFIG_KEXEC_HARDBOOT
extern unsigned long kexec_hardboot;
void (*kexec_hardboot_hook)(void);
#endif

/*
 * Provide a dummy crash_notes definition while crash dump arrives to arm.
 * This prevents breakage of crash_notes attribute in kernel/ksysfs.c.
 */

int machine_kexec_prepare(struct kimage *image)
{
	return 0;
}

void machine_kexec_cleanup(struct kimage *image)
{
}

void machine_shutdown(void)
{
}

void machine_crash_shutdown(struct pt_regs *regs)
{
}

void machine_kexec(struct kimage *image)
{
	unsigned long page_list;
	unsigned long reboot_code_buffer_phys;
	void *reboot_code_buffer;


	page_list = image->head & PAGE_MASK;

	/* we need both effective and real address here */
	reboot_code_buffer_phys =
	    page_to_pfn(image->control_code_page) << PAGE_SHIFT;
	reboot_code_buffer = page_address(image->control_code_page);

	/* Prepare parameters for reboot_code_buffer*/
	kexec_start_address = image->start;
	kexec_indirection_page = page_list;
	kexec_mach_type = machine_arch_type;
	kexec_boot_atags = image->start - KEXEC_ARM_ZIMAGE_OFFSET + KEXEC_ARM_ATAGS_OFFSET;
#ifdef CONFIG_KEXEC_HARDBOOT
	kexec_hardboot = image->hardboot;
#endif


	/* copy our kernel relocation code to the control code page */
	memcpy(reboot_code_buffer,
	       relocate_new_kernel, relocate_new_kernel_size);


	flush_icache_range((unsigned long) reboot_code_buffer,
			   (unsigned long) reboot_code_buffer + KEXEC_CONTROL_PAGE_SIZE);
	printk(KERN_INFO "Bye!\n");

	local_irq_disable();
	local_fiq_disable();

	setup_mm_for_reboot(0); /* mode is not used, so just pass 0*/

#ifdef CONFIG_KEXEC_HARDBOOT
	if (image->hardboot && kexec_hardboot_hook)
		/* Run any final machine-specific shutdown code. */
		kexec_hardboot_hook();
#endif

#ifdef CONFIG_ARCH_S5PV310
	/* Must flush the outer cache for the relocation code to appear at its
	 * physical address, otherwise jumping to reboot_code_buffer_phys fails.
	 * Note: Calling flush_cache_all() after cpu_proc_fin() causes a CPU
	 * hang, as does calling outer_disable() and outer_inv_all(). */
	flush_cache_all();
	outer_flush_all();
	cpu_proc_fin();
#else
	flush_cache_all();
	cpu_proc_fin();
	flush_cache_all();
#endif

	/* Must call cpu_reset via physical address since ARMv7 (& v6) stalls the
	 * pipeline after disabling the MMU. */
	((typeof(cpu_reset) *)virt_to_phys(cpu_reset))(reboot_code_buffer_phys);
}
