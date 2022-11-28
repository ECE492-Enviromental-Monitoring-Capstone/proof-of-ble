GEN_DIR="./gen"
mkdir -p ${GEN_DIR}

gdbus-codegen --c-namespace Bluez_Service \
	--glib-min-required 2.66 \
	--c-generate-autocleanup all \
	--c-generate-object-manager \
	--header --output ${GEN_DIR}/bluez_service.h \
	./bluez_gattservice.xml
gdbus-codegen --c-namespace Bluez_Service \
	--glib-min-required 2.66 \
	--c-generate-autocleanup all \
	--c-generate-object-manager \
	--body --output ${GEN_DIR}/bluez_service.c \
	./bluez_gattservice.xml

gdbus-codegen --c-namespace Bluez_Characteristic \
	--glib-min-required 2.66 \
	--c-generate-autocleanup all \
	--c-generate-object-manager \
	--header --output ${GEN_DIR}/bluez_characteristic.h \
	./bluez_gattcharacteristic.xml
gdbus-codegen --c-namespace Bluez_Characteristic \
	--glib-min-required 2.66 \
	--c-generate-autocleanup all \
	--c-generate-object-manager \
	--body --output ${GEN_DIR}/bluez_characteristic.c \
	./bluez_gattcharacteristic.xml

gdbus-codegen --c-namespace Bluez_Advertisement \
	--glib-min-required 2.66 \
	--c-generate-autocleanup all \
	--c-generate-object-manager \
	--header --output ${GEN_DIR}/bluez_advertisement.h \
	./bluez_advertisement.xml
gdbus-codegen --c-namespace Bluez_Advertisement \
	--glib-min-required 2.66 \
	--c-generate-autocleanup all \
	--c-generate-object-manager \
	--body --output ${GEN_DIR}/bluez_advertisement.c \
	./bluez_advertisement.xml

