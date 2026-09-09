package main

import (
	"context"
	"net"
	"net/http"

	"github.com/matsuridayo/libneko/neko_common"
	"github.com/matsuridayo/libneko/neko_log"
	box "github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/boxapi"
)

var instance *box.Box
var instance_cancel context.CancelFunc
var instance_stats *boxapi.SbV2rayServer
var instance_conn *nekoConnTracker

func setupCore() {
	//
	neko_log.SetupLog(50*1024, "./neko.log")
	//
	neko_common.GetCurrentInstance = func() interface{} {
		return instance
	}
	neko_common.DialContext = func(ctx context.Context, specifiedInstance interface{}, network, addr string) (net.Conn, error) {
		if i, ok := specifiedInstance.(*box.Box); ok {
			return boxapi.DialContext(ctx, i, nil, network, addr)
		}
		if instance != nil {
			return boxapi.DialContext(ctx, instance, currentTracker(), network, addr)
		}
		return neko_common.DialContextSystem(ctx, network, addr)
	}
	neko_common.DialUDP = func(ctx context.Context, specifiedInstance interface{}) (net.PacketConn, error) {
		if i, ok := specifiedInstance.(*box.Box); ok {
			return boxapi.DialUDP(ctx, i)
		}
		if instance != nil {
			return boxapi.DialUDP(ctx, instance)
		}
		return neko_common.DialUDPSystem(ctx)
	}
	neko_common.CreateProxyHttpClient = func(specifiedInstance interface{}) *http.Client {
		if i, ok := specifiedInstance.(*box.Box); ok {
			return boxapi.CreateProxyHttpClient(i, nil)
		}
		return boxapi.CreateProxyHttpClient(instance, currentTracker())
	}
}

func currentTracker() adapter.ConnectionTracker {
	if instance_stats != nil {
		return instance_stats.StatsService()
	}
	return nil
}
