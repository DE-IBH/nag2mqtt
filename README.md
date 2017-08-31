# nag2mqtt

_Nagios event broker to MQTT gateway_


## About

nag2mqtt consists of a Nagios Event Broker (NEB) module and a small perl
daemon. The NEB module publishes all check results in the local filesystem
(using tmpfs is highly recommended). These file are than publish by the
perl daemon to a MQTT broker.

The published check results could be used by websites using a MQTT WebSocket
API or tools like [mqttwarn](https://github.com/jpmens/mqttwarn).

*The Nagios header files in `external/nagios-3.5.0` are taken from the
Nagios Core 3.5.0 sources. This should work fine with Check_MK.*


## Dependencies

#### NEB module (neb2mqtt.so):
- [libjson-c](https://github.com/json-c/json-c)
- [libmhash](http://mhash.sourceforge.net/)
- build:
  - pkg-config
  - header files for both libraries

#### Publisher (nag2mqttd):
- AnyEvent
- AnyEvent::MQTT
- JSON
- Linux::Inotify2
- Log::Dispatch
- Proc::PID::File


## Install

Details on installing nag2mqtt can be found in the documentation of
[SNMD](http://snmd.readthedocs.io/en/latest/appx_nag2mqtt.html).
