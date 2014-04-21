#include <linux/module.h>
#include <linux/pci.h>

#include "sonic.h"
#include "tester.h"

MODULE_AUTHOR("Ki Suh Lee: kslee@cs.cornell.edu");
MODULE_DESCRIPTION("Sonic driver");
MODULE_LICENSE("Dual BSD/GPL");

static int __sonic_init(struct sonic_priv **);
static void __sonic_exit(struct sonic_priv *);

struct sonic_priv *sonic;
#if !SONIC_PCIE
#else /*SONIC_PCIE*/
static int __devinit sonic_probe(struct pci_dev *, const struct pci_device_id *);
static void sonic_remove(struct pci_dev * pdev);

static struct pci_device_id sonic_pci_tbl[] = {
	{ PCI_DEVICE(SONIC_PCI_VENDOR_ID, SONIC_PCI_DEVICE_ID) },
	{ 0 },
};

static struct pci_driver sonic_driver = {
	.name = SONIC_NAME,
	.id_table = sonic_pci_tbl,
	.probe = sonic_probe,
	.remove = sonic_remove,
};

MODULE_DEVICE_TABLE(pci, sonic_pci_tbl);

static irqreturn_t sonic_intr(int irq, void *arg)
{
	struct sonic_priv * sonic = arg;
#if SONIC_DDEBUG
	struct sonic_irq_data * irq_data = sonic->irq_data;
#endif
	
	sonic->irq_count++;
	SONIC_DDPRINT("%lld irq->rx_ptr %x\n", sonic->irq_count, irq_data->rx_ptr);

	return IRQ_HANDLED;
}

static int sonic_request_irq(struct sonic_priv * sonic)
{
	int status=0;
	struct pci_dev *pdev = sonic->pdev;
	sonic->msi_enabled = 0;

	SONIC_PRINT("sonic_request_irq %d\n", pdev->irq);

	// For now, let's use msi
	status = pci_enable_msi(pdev);
	if (status != 0) 
		SONIC_ERROR("Error %d setting up MSI\n", status);
	else 
		sonic->msi_enabled = 1;	

	status = request_irq(pdev->irq, sonic_intr, IRQF_SHARED,
				SONIC_NAME, sonic);		
	if (status != 0) {
		SONIC_ERROR("Failed to allocate IRQ\n");
		if (sonic->msi_enabled) 
			goto abort;
	}

	SONIC_PRINT("IRQ %d enabled\n", pdev->irq);
	return 0;

abort:
	pci_disable_msi(pdev);

	return status;
}

static void sonic_free_irq(struct sonic_priv *sonic)
{
	struct pci_dev *pdev = sonic->pdev;

	SONIC_PRINT("sonic_free_irq %d\n", pdev->irq);

	free_irq(pdev->irq, sonic);
	if(sonic->msi_enabled)
		pci_disable_msi(pdev);
}

static int __devinit sonic_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int status = -ENXIO;
	struct sonic_priv *sonic;

	SONIC_PRINT("\n");

	if ((status = __sonic_init(&sonic))) {
		SONIC_ERROR("__sonic_init failed\n");
		goto abort;
	}
	SONIC_DPRINT("__sonic_init succedded\n");

	sonic->pdev = pdev;

	if (pci_enable_device(pdev)) {
		SONIC_ERROR("pci_enable_device failed\n");
		status = -ENODEV;
		goto abort_sonic;
	}
	SONIC_DPRINT("pci enabled\n");

	pci_set_master(pdev);

	if ((status = sonic_request_irq(sonic)))
		goto abort_pci;

	if((status = pci_request_regions(pdev, SONIC_NAME))) { // TODO: Is this necessary?
		SONIC_ERROR ("pci_request_regions failed\n");
		goto abort_irq;
	}

	status = pci_set_dma_mask(pdev, DMA_64BIT_MASK);
	if (status != 0) {
		SONIC_ERROR ("pci_set_dma_mask 64bit failed\n");
		status = pci_set_dma_mask(pdev, DMA_32BIT_MASK);
	}
	if (status != 0) {
		SONIC_ERROR ("pci_set_dma_mask 32bit failed\n");
		goto abort_region;
	}	

	pci_set_consistent_dma_mask(pdev, DMA_64BIT_MASK);

#if SONIC_DDEBUG
	sonic_scan_bars(sonic);
#endif

	if ((status = sonic_alloc_ports(sonic)))
		goto abort_region;

//	if((status = sonic_map_bars(sonic)))
//		goto abort_region;

	/* control register */
//	sonic->ports[0]->ctrl = sonic->bar[2];
//	SONIC_DPRINT("port[0] bar is mapped %p\n", sonic->ports[0]->ctrl);
#if SONIC_TWO_PORTS
//	sonic->ports[1]->ctrl = sonic->bar[3];
//	SONIC_DPRINT("port[1] bar is mapped %p\n", sonic->ports[1]->ctrl);
#endif

//	if((status = sonic_fpga_init(sonic)))
//		goto abort_bars;

	pci_set_drvdata(pdev, sonic);

	SONIC_PRINT("sonic probe successed\n");
	return 0;
//abort_bars:
//	sonic_unmap_bars(sonic);
abort_region:
	pci_release_regions(pdev);
abort_irq:
	sonic_free_irq(sonic);	
abort_pci:
	pci_disable_device(pdev);	
abort_sonic:
	__sonic_exit(sonic);
abort:
	return status;
}

static void sonic_remove(struct pci_dev * pdev)
{
	struct sonic_priv *sonic;
	SONIC_PRINT("\n");

	if ((sonic = pci_get_drvdata(pdev)) == NULL)
		return;

//	sonic_fpga_exit(sonic);

//	sonic_unmap_bars(sonic);

	pci_release_regions(pdev);

	sonic_free_irq(sonic);

	pci_disable_device(pdev);

	__sonic_exit(sonic);

	pci_set_drvdata(pdev, NULL);

	SONIC_PRINT("sonic removed\n");
}
#endif /*SONIC_PCIE*/

static int __sonic_init(struct sonic_priv **psonic)
{
	struct sonic_priv * s;
	int status = 0;
	SONIC_PRINT("\n");

	s= kzalloc(sizeof(struct sonic_priv), GFP_KERNEL);
	if (s == NULL) {
		SONIC_ERROR("Memory allocation failed\n");
		status = -ENOMEM;
		goto abort;
	}

	sonic_init_tester_args(&s->tester_args);

	spin_lock_init(&s->lock);

#if SONIC_PROC
	if((status = sonic_proc_init(s)))
		goto abort_sonic;
#endif
#if SONIC_FS
	if((status = sonic_fs_init(s, SONIC_PORT_SIZE)))
		goto abort_proc;
#endif

//	init_waitqueue_head(&sonic->wait);

	*psonic = s;
	sonic = s;
	return 0;

#if SONIC_FS
abort_proc:
#endif
#if SONIC_PROC
	sonic_proc_exit(sonic);
abort_sonic:
#endif
	kfree(sonic);
abort:
	return status;
}

static void __sonic_exit(struct sonic_priv *sonic)
{
	SONIC_PRINT("\n");
#if SONIC_FS
	sonic_fs_exit(sonic);
#endif /*SONIC_FS*/
#if SONIC_PROC
	sonic_proc_exit(sonic);
#endif /*SONIC_PROC*/
//	sonic_free_ports(sonic);

	kfree(sonic);
}

static int __init sonic_init(void)
{
	int status = 0;
	SONIC_PRINT("\n");

#if SONIC_PCIE
	status = pci_register_driver(&sonic_driver);
#else
	if((status = __sonic_init(&sonic))) {
		SONIC_ERROR("__sonic_init failed\n");
		return status;
	}

	if((status = sonic_alloc_ports(sonic)))
		__sonic_exit(sonic);
#endif
	if (status != 0) 
		SONIC_ERROR("sonic_init failed\n");

	return status;
}

static void __exit sonic_exit(void)
{
	SONIC_PRINT("\n");
#if SONIC_PCIE
	pci_unregister_driver(&sonic_driver);
#else
	sonic_free_ports(sonic);

	__sonic_exit(sonic);
#endif
	SONIC_PRINT("sonic_done\n");
}

module_init(sonic_init);
module_exit(sonic_exit);
