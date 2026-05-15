package service

import (
	"context"
	"errors"
	"log"

	pb "bubble/api/bubble/v1"
	biz "bubble/internal/biz"

	"google.golang.org/protobuf/types/known/emptypb"
)

type BubbleService struct {
	pb.UnimplementedBubbleServer

	uc *biz.EventUsecase
}

func NewBubbleService(uc *biz.EventUsecase) *BubbleService {
	return &BubbleService{uc: uc}
}

func (s *BubbleService) CreateBubble(ctx context.Context, req *pb.CreateBubbleRequest) (*pb.CreateBubbleReply, error) {
	log.Println("Service: create event", req.Event)
	event := req.GetEvent()
	if event != nil {
		model := &biz.EventModel{ID: event.Id, Title: event.Title, Status: event.Status}
		model, err := s.uc.CreateEventModel(ctx, model)
		if err != nil {
			return &pb.CreateBubbleReply{}, errors.New("create event failed, biz error")
		}
		return &pb.CreateBubbleReply{Event: &pb.Event{Id: model.ID, Title: model.Title, Status: model.Status}}, nil
	}
	return &pb.CreateBubbleReply{}, errors.New("create event failed, event is nil")
}
func (s *BubbleService) UpdateBubble(ctx context.Context, req *pb.UpdateBubbleRequest) (*pb.UpdateBubbleReply, error) {
	log.Println("Service: update event", req.Event)
	event := req.GetEvent()
	if event != nil {
		model := &biz.EventModel{ID: event.Id, Title: event.Title, Status: event.Status}
		model, err := s.uc.UpdateEventModel(ctx, model)
		if err != nil {
			return &pb.UpdateBubbleReply{}, errors.New("update event failed, biz error")
		}
		return &pb.UpdateBubbleReply{Event: &pb.Event{Id: model.ID, Title: model.Title, Status: model.Status}}, nil
	}
	return &pb.UpdateBubbleReply{}, nil
}
func (s *BubbleService) DeleteBubble(ctx context.Context, req *pb.DeleteBubbleRequest) (*emptypb.Empty, error) {
	log.Println("Service: delete event", req.Event)
	event := req.GetEvent()
	if event != nil {
		model := &biz.EventModel{ID: event.Id, Title: event.Title, Status: event.Status}
		err := s.uc.DeleteEventModel(ctx, model)
		if err != nil {
			return &emptypb.Empty{}, errors.New("delete event failed, biz error")
		}
	}
	return &emptypb.Empty{}, nil
}
func (s *BubbleService) ListBubble(ctx context.Context, req *emptypb.Empty) (*pb.ListBubbleReply, error) {
	log.Println("Service: list event")
	models, err := s.uc.ListAll(ctx)
	if err != nil {
		return nil, errors.New("list event failed, biz error")
	}
	events := make([]*pb.Event, len(models))
	for i, model := range models {
		events[i] = &pb.Event{Id: model.ID, Title: model.Title, Status: model.Status}
	}
	return &pb.ListBubbleReply{Events: events}, nil
}
