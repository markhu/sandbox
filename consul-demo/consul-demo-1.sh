
set -x
  312  consul members
  318  consul info
  381  consul version
  400  sudo monit -d 9
  408  vim /tmp/consul-monit-conf/consul-webui.json 
  409  consul reload
  416  ps -ef | grep monit
  420  sudo monit quit
  421  curl localhost:8500/v1/session/node/
  422  curl localhost:8500/v1/session/
  423  curl -isS  localhost:8500/v1/session/
  424  curl -isS  localhost:8500/v1/
  425  curl -isS  localhost:8500/v1/session/nodes
  426  curl -isS localhost:8500/v1/session/services
  427  curl -isS localhost:8500/v1/session/servicessdf
  428  curl -isS localhost:8500/v1/session/?
  429  curl -isS localhost:8500/v1/session/help
  430  curl -isS localhost:8500/v1/help
  431  curl -isS localhost:8500/v1/services
  432  curl -isS localhost:8500/v1/agent
  433  curl -isS localhost:8500/v1/agent/self
  434  curl -isS localhost:8500/v1/agent/self | jq .
  435  sudo yum install jq
  436  curl -isS localhost:8500/v1/agent/self | jq .
  437  curl -sS localhost:8500/v1/agent/self | jq .
  438  curl -sS localhost:8500/v1/agent/self | jq .Stats
  439  curl -sS localhost:8500/v1/agent/self | jq .Stats.consul
  440  curl localhost:2812
  441  curl -sS localhost:8500/v1/status
  442  curl -sS localhost:8500/v1/status/leader
  443  curl -sS localhost:8500/v1/status/peers
  444  curl -sS localhost:8500/v1/session/list
  445  curl -sS localhost:8500/v1/operator
  446  curl -sS localhost:8500/v1/operator/raft/peer
  447  curl -sS localhost:8500/v1/operator/raft/configuration
  448  curl -sS localhost:8500/v1/coordinate/nodes
  449  curl -sS localhost:8500/v1/coordinate/nodes | jq .
  450  curl -sS localhost:8500/v1/agent/ | jq .
  451  curl -sS localhost:8500/v1/agent jq .
  452  curl -sS localhost:8500/v1/agent | jq .
  453  curl -sS localhost:8500/v1/agent/check | jq .
  454  curl -sS localhost:8500/v1/agent/checks | jq .
  455  curl -sS localhost:8500/v1/agent/services | jq .
  456  curl -sS localhost:8500/v1/agent/monitor | jq .
  457  curl -sS localhost:8500/v1/agent/monitor 
  458  curl -sS localhost:8500/v1/agent/self | jq .
  459  curl -sS localhost:8500/v1/agent/services | jq .
  460  curl -sS localhost:8500/v1/agent/checks | jq .
  461  dig @127.0.0.1 -p 8600 web.service.consul
  462  dig @127.0.0.1 -p 8600 monit.service.consul
  463  dig @127.0.0.1 -p 8600 monit.service.consul SRV
  464  dig @127.0.0.1 -p 8600 monit.service.consul
  465  dig @127.0.0.1 -p 8600 monit.service.consul SRV
  466  dig @127.0.0.1 -p 8600 frpq-mec9000.service.consul
  467  dig @127.0.0.1 -p 8600 frpq-mec9000.lab1.fanops.net.node.dc1.consul
  468  dig @127.0.0.1 -p 8600 frpq-mec9000.lab1.fanops.net
  469  dig @127.0.0.1 -p 8600 frpq-mec9000
  470  curl -sS localhost:8500/v1/agent/services | jq .
  471  dig @127.0.0.1 -p 8600 monit.service.consul
  472  dig @127.0.0.1 -p 8600 monit.test.service.consul
  473  dig @127.0.0.1 -p 8600 test.service.consul
  474  dig @127.0.0.1 -p 8600 test.monit.service.consul
  475  curl -sS localhost:8500/v1/agent/services | jq .
  476  curl -sS localhost:8500/v1/agent/service/monit | jq .
# vim -o /tmp/consul-monit-conf/consul-*
consul reload
# sudo monit -d 9
  curl -sS localhost:8500/v1/catalog/check/monit | jq .
curl -sS localhost:8500/v1/catalog/service/monit | jq .
curl -sS localhost:8500/v1/agent/services | jq .
curl -sS localhost:8500/v1/agent/checks | jq .
curl -sS localhost:8500/v1/health/checks/monit | jq .
consul leave
