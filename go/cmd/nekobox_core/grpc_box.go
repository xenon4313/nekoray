package main

import (
	"context"
	"errors"
	"fmt"
	"io"
	"log"

	"grpc_server"
	"grpc_server/gen"

	"github.com/matsuridayo/libneko/neko_common"
	"github.com/matsuridayo/libneko/neko_log"
	"github.com/matsuridayo/libneko/speedtest"
	box "github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/boxapi"
	"github.com/sagernet/sing-box/include"
	singlog "github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common/json"
)

type nekoLogWriter struct {
	w io.Writer
}

func (n *nekoLogWriter) WriteMessage(level singlog.Level, message string) {
	if n.w != nil {
		n.w.Write([]byte(message + "\n"))
	}
}

func createBox(ctx context.Context, configBytes []byte) (*box.Box, context.CancelFunc, error) {
	baseCtx := include.Context(context.Background())

	options, err := json.UnmarshalExtendedContext[option.Options](baseCtx, configBytes)
	if err != nil {
		return nil, nil, err
	}
	if options.Log == nil {
		options.Log = &option.LogOptions{}
	}
	options.Log.DisableColor = true

	boxCtx, cancel := context.WithCancel(baseCtx)
	instance, err := box.New(box.Options{
		Context:           boxCtx,
		Options:           options,
		PlatformLogWriter: &nekoLogWriter{w: neko_log.LogWriter},
	})
	if err != nil {
		cancel()
		return nil, nil, err
	}
	err = instance.Start()
	if err != nil {
		cancel()
		return nil, nil, err
	}
	return instance, cancel, nil
}

type server struct {
	grpc_server.BaseServer
}

func (s *server) Start(ctx context.Context, in *gen.LoadConfigReq) (out *gen.ErrorResp, _ error) {
	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = err.Error()
			instance = nil
			instance_stats = nil
			instance_conn = nil
		}
	}()

	if neko_common.Debug {
		log.Println("Start:", in.CoreConfig)
	}

	if instance != nil {
		err = errors.New("instance already started")
		return
	}

	instance, instance_cancel, err = createBox(ctx, []byte(in.CoreConfig))

	if instance != nil {
		// Connection details tracker (process / FQDN / IP)
		instance_conn = newNekoConnTracker(instance.Outbound())
		instance.Router().AppendTracker(instance_conn)
		// V2ray Service / connection tracker
		if in.StatsOutbounds != nil {
			instance_stats = boxapi.NewSbV2rayServer(option.V2RayStatsServiceOptions{
				Enabled:   true,
				Outbounds: in.StatsOutbounds,
			})
			instance.Router().AppendTracker(instance_stats.StatsService())
		}
	}

	return
}

func (s *server) Stop(ctx context.Context, in *gen.EmptyReq) (out *gen.ErrorResp, _ error) {
	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = err.Error()
		}
	}()

	if instance == nil {
		return
	}

	instance_cancel()
	instance.Close()

	instance = nil
	instance_stats = nil
	instance_conn = nil

	return
}

func (s *server) Test(ctx context.Context, in *gen.TestReq) (out *gen.TestResp, _ error) {
	var err error
	out = &gen.TestResp{Ms: 0}

	defer func() {
		if err != nil {
			out.Error = err.Error()
		}
	}()

	if in.Mode == gen.TestMode_UrlTest {
		var i *box.Box
		var cancel context.CancelFunc
		if in.Config != nil {
			// Test instance
			i, cancel, err = createBox(ctx, []byte(in.Config.CoreConfig))
			if i != nil {
				defer i.Close()
				defer cancel()
			}
			if err != nil {
				return
			}
		} else {
			// Test running instance
			i = instance
			if i == nil {
				return
			}
		}
		// Latency
		out.Ms, err = speedtest.UrlTest(boxapi.CreateProxyHttpClient(i, currentTracker()), in.Url, in.Timeout, speedtest.UrlTestStandard_RTT)
	} else if in.Mode == gen.TestMode_TcpPing {
		out.Ms, err = speedtest.TcpPing(in.Address, in.Timeout)
	} else if in.Mode == gen.TestMode_FullTest {
		var cancel context.CancelFunc
		var i *box.Box
		i, cancel, err = createBox(ctx, []byte(in.Config.CoreConfig))
		if i != nil {
			defer i.Close()
			defer cancel()
		}
		if err != nil {
			return
		}
		return grpc_server.DoFullTest(ctx, in, i)
	}

	return
}

func (s *server) QueryStats(ctx context.Context, in *gen.QueryStatsReq) (out *gen.QueryStatsResp, _ error) {
	out = &gen.QueryStatsResp{}

	if instance_stats != nil {
		out.Traffic = instance_stats.QueryStats(fmt.Sprintf("outbound>>>%s>>>traffic>>>%s", in.Tag, in.Direct))
	}

	return
}

func (s *server) ListConnections(ctx context.Context, in *gen.EmptyReq) (*gen.ListConnectionsResp, error) {
	out := &gen.ListConnectionsResp{}
	if instance_conn != nil {
		out.NekorayConnectionsJson = instance_conn.listJSON()
	} else {
		out.NekorayConnectionsJson = "[]"
	}
	return out, nil
}
