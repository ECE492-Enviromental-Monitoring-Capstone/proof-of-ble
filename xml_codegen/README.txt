This folder hosts xml files used for GDBUS codegen. These XML files layout the various dbus interfaces
needed in order to use bluez.

The service we need to use is located on the system dbus.
The well know name of this services is "org.bluez"
The particular object we are interested in is located at path "/org/bluez/hci0"
Its introspection is as follows:

Command:
gdbus introspect --system --dest "org.bluez" --object-path "/org/bluez/hci0"

node /org/bluez/hci0 {
  interface org.freedesktop.DBus.Introspectable {
    methods:
      Introspect(out s xml);
    signals:
    properties:
  };
  interface org.bluez.Adapter1 {
    methods:
      StartDiscovery();
      SetDiscoveryFilter(in  a{sv} properties);
      StopDiscovery();
      RemoveDevice(in  o device);
      GetDiscoveryFilters(out as filters);
      ConnectDevice(in  a{sv} properties);
    signals:
    properties:
      readonly s Address = 'B8:27:EB:DE:35:F4';
      readonly s AddressType = 'public';
      readonly s Name = 'raspberrypi';
      readwrite s Alias = 'raspberrypi';
      readonly u Class = 0;
      readwrite b Powered = true;
      readwrite b Discoverable = false;
      readwrite u DiscoverableTimeout = 180;
      readwrite b Pairable = false;
      readwrite u PairableTimeout = 0;
      readonly b Discovering = false;
      readonly as UUIDs = ['00001801-0000-1000-8000-00805f9b34fb', '00001800-0000-1000-8000-00805f9b34fb', '00001200-0000-1000-8000-00805f9b34fb', '0000110c-0000-1000-8000-00805f9b34fb', '0000110e-0000-1000-8000-00805f9b34fb', '0000180a-0000-1000-8000-00805f9b34fb'];
      readonly s Modalias = 'usb:v1D6Bp0246d0540';
      readonly as Roles = ['central', 'peripheral', 'central-peripheral'];
      readonly as ExperimentalFeatures = ['671b10b5-42c0-4696-9227-eb28d1b049d6'];
  };
  interface org.freedesktop.DBus.Properties {
    methods:
      Get(in  s interface,
          in  s name,
          out v value);
      Set(in  s interface,
          in  s name,
          in  v value);
      GetAll(in  s interface,
             out a{sv} properties);
    signals:
      PropertiesChanged(s interface,
                        a{sv} changed_properties,
                        as invalidated_properties);
    properties:
  };
  interface org.bluez.BatteryProviderManager1 {
    methods:
      RegisterBatteryProvider(in  o provider);
      UnregisterBatteryProvider(in  o provider);
    signals:
    properties:
  };
  interface org.bluez.GattManager1 {
    methods:
      RegisterApplication(in  o application,
                          in  a{sv} options);
      UnregisterApplication(in  o application);
    signals:
    properties:
  };
  interface org.bluez.AdvertisementMonitorManager1 {
    methods:
      RegisterMonitor(in  o application);
      UnregisterMonitor(in  o application);
    signals:
    properties:
      readonly as SupportedMonitorTypes = ['or_patterns'];
      readonly as SupportedFeatures = [];
  };
  interface org.bluez.Media1 {
    methods:
      RegisterEndpoint(in  o endpoint,
                       in  a{sv} properties);
      UnregisterEndpoint(in  o endpoint);
      RegisterPlayer(in  o player,
                     in  a{sv} properties);
      UnregisterPlayer(in  o player);
      RegisterApplication(in  o application,
                          in  a{sv} options);
      UnregisterApplication(in  o application);
    signals:
    properties:
  };
  interface org.bluez.NetworkServer1 {
    methods:
      Register(in  s uuid,
               in  s bridge);
      Unregister(in  s uuid);
    signals:
    properties:
  };
  interface org.bluez.LEAdvertisingManager1 {
    methods:
      RegisterAdvertisement(in  o advertisement,
                            in  a{sv} options);
      UnregisterAdvertisement(in  o service);
    signals:
    properties:
      readonly y ActiveInstances = 0x00;
      readonly y SupportedInstances = 0x05;
      readonly as SupportedIncludes = ['tx-power', 'appearance', 'local-name'];
      readonly as SupportedSecondaryChannels;
      readonly as SupportedFeatures = [];
      readonly a{sv} SupportedCapabilities = {'MaxAdvLen': <byte 0x1f>, 'MaxScnRspLen': <byte 0x1f>};
  };
};


We are specifically interested only in its "org.bluez.LEAdvertisingManager1" and
its "org.bluez.GattManager1" interfaces. Using --xml in the command above we generate XML
files and remove all of the cruft aside from our desired interfaces.

The file bluez_hci_introspect.xml is what we get out of this process.

Now we need three other xml files for code generation. We need these three objects in particular:

https://git.kernel.org/pub/scm/bluetooth/bluez.git/tree/doc/advertising-api.txt
org.bluez.LEAdvertisement1

https://git.kernel.org/pub/scm/bluetooth/bluez.git/tree/doc/gatt-api.txt
org.bluez.GattService1
org.bluez.GattCharacteristic1

A manager object is also needed on which the services and characteristics are managed by.
This needs to support org.freedesktop.DBus.ObjectManager however this is done for us automatically
by some automagic parts of Gio.



