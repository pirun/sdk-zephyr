#!//usr/bin/bash
CENTRAL_SNR=1050002933
PERIPHERAL_SNR=683892318
SAMPLE_FOLDER=samples/bluetooth
pushd ${SAMPLE_FOLDER}
west build periodic_adv_rsp_conn -p -b nrf5340dk_nrf5340_cpuapp -d periodic_adv_rsp_conn/build --pristine
west build periodic_sync_rsp_conn -p -b nrf52840dk_nrf52840 -d periodic_sync_rsp_conn/build  --pristine
west flash -d periodic_adv_rsp_conn/build --erase -i ${CENTRAL_SNR}
west flash -d periodic_sync_rsp_conn/build --erase -i ${PERIPHERAL_SNR}
popd
