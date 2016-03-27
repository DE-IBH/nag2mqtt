# nag2mqtt

_Nagios event broker to MQTT gateway_


## About

nag2mqtt consists of a Nagios Event Broker (NEB) module and a small perl
daemon. The NEB module publishes all check results in the local filesystem
(using tmpfs is highly recommended). These file are than publish by the
perl daemon to a MQTT broker.

The published check results could be used by websites using a MQTT WebSocket
API or tools like [mqttwarn](https://github.com/jpmens/mqttwarn).


## Dependencies

#### NEB module (neb2mqtt.so):
- [libjson-c](https://github.com/json-c/json-c)
- [libmhash](http://mhash.sourceforge.net/)
- build:
  - pkg-config
  - header files from [nagioscore](https://github.com/NagiosEnterprises/nagioscore), use `git submodule init`

#### Publisher (nag2mqttd):
- AnyEvent
- AnyEvent::MQTT
- JSON
- Linux::Inotify2
- Log::Dispatch
- Proc::PID::File
