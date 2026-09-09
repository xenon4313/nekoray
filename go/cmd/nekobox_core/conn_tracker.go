package main

import (
	"context"
	"encoding/json"
	"net"
	"path/filepath"
	"strconv"
	"time"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/trafficcontrol"
	"github.com/sagernet/sing-tun"
	N "github.com/sagernet/sing/common/network"
)

type nekoConnTracker struct {
	manager *trafficcontrol.Manager
}

func newNekoConnTracker(outbound adapter.OutboundManager) *nekoConnTracker {
	m := trafficcontrol.NewManager(outbound)
	_ = m.Start(adapter.StartStateInitialize)
	return &nekoConnTracker{
		manager: m,
	}
}

func (t *nekoConnTracker) RoutedConnection(ctx context.Context, conn net.Conn, metadata adapter.InboundContext, matchedRule adapter.Rule, matchOutbound adapter.Outbound) net.Conn {
	return t.manager.RoutedConnection(ctx, conn, metadata, matchedRule, matchOutbound)
}

func (t *nekoConnTracker) RoutedPacketConnection(ctx context.Context, conn N.PacketConn, metadata adapter.InboundContext, matchedRule adapter.Rule, matchOutbound adapter.Outbound) N.PacketConn {
	return t.manager.RoutedPacketConnection(ctx, conn, metadata, matchedRule, matchOutbound)
}

func (t *nekoConnTracker) RoutedFlow(ctx context.Context, metadata adapter.InboundContext, matchedRule adapter.Rule, matchOutbound adapter.Outbound) tun.FlowTracker {
	return t.manager.RoutedFlow(ctx, metadata, matchedRule, matchOutbound)
}

type nekoConnJSON struct {
	ID          string `json:"ID"`
	Tag         string `json:"Tag"`
	Start       int64  `json:"Start"`
	End         int64  `json:"End"`
	Dest        string `json:"Dest"`
	RDest       string `json:"RDest"`
	Host        string `json:"Host"`
	DestIP      string `json:"DestIP"`
	DestPort    int    `json:"DestPort"`
	Network     string `json:"Network"`
	Process     string `json:"Process"`
	ProcessPath string `json:"ProcessPath"`
	Upload      int64  `json:"Upload"`
	Download    int64  `json:"Download"`
	Active      bool   `json:"Active"`
}

func metadataToJSON(meta *trafficcontrol.TrackerMetadata, active bool) nekoConnJSON {
	host := meta.Metadata.Domain
	if host == "" {
		host = meta.Metadata.Destination.Fqdn
	}
	destIP := ""
	if meta.Metadata.Destination.IsIP() {
		destIP = meta.Metadata.Destination.Addr.String()
	} else if len(meta.Metadata.DestinationAddresses) > 0 {
		destIP = meta.Metadata.DestinationAddresses[0].String()
	}
	destPort := int(meta.Metadata.Destination.Port)

	processPath := ""
	processName := ""
	if meta.Metadata.ProcessInfo != nil {
		processPath = meta.Metadata.ProcessInfo.ProcessPath
		if processPath != "" {
			processName = filepath.Base(processPath)
		} else if len(meta.Metadata.ProcessInfo.AndroidPackageNames) > 0 {
			processName = meta.Metadata.ProcessInfo.AndroidPackageNames[0]
			processPath = processName
		}
	}
	if processName == "" {
		processName = "(unknown)"
	}

	displayHost := host
	if displayHost == "" {
		if destIP != "" {
			displayHost = destIP
		} else {
			displayHost = meta.Metadata.Destination.String()
		}
	}

	dest := displayHost
	if destPort > 0 {
		dest = net.JoinHostPort(displayHost, strconv.Itoa(destPort))
	}
	rdest := destIP
	if rdest != "" && destPort > 0 {
		rdest = net.JoinHostPort(destIP, strconv.Itoa(destPort))
	}

	tag := meta.Outbound
	if tag == "" && len(meta.Chain) > 0 {
		tag = meta.Chain[0]
	}

	end := int64(0)
	if !active && !meta.ClosedAt.IsZero() {
		end = meta.ClosedAt.Unix()
	}

	var up, down int64
	if meta.Upload != nil {
		up = meta.Upload.Load()
	}
	if meta.Download != nil {
		down = meta.Download.Load()
	}

	return nekoConnJSON{
		ID:          meta.ID.String(),
		Tag:         tag,
		Start:       meta.CreatedAt.Unix(),
		End:         end,
		Dest:        dest,
		RDest:       rdest,
		Host:        displayHost,
		DestIP:      destIP,
		DestPort:    destPort,
		Network:     meta.Metadata.Network,
		Process:     processName,
		ProcessPath: processPath,
		Upload:      up,
		Download:    down,
		Active:      active,
	}
}

func (t *nekoConnTracker) listJSON() string {
	if t == nil || t.manager == nil {
		return "[]"
	}
	out := make([]nekoConnJSON, 0, 128)
	for _, meta := range t.manager.Connections() {
		out = append(out, metadataToJSON(meta, true))
	}
	for _, meta := range t.manager.ClosedConnections() {
		if !meta.ClosedAt.IsZero() && time.Since(meta.ClosedAt) > 2*time.Minute {
			continue
		}
		out = append(out, metadataToJSON(meta, false))
	}
	b, err := json.Marshal(out)
	if err != nil {
		return "[]"
	}
	return string(b)
}
