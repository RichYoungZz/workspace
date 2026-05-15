package biz

import (
	"context"

	log "github.com/go-kratos/kratos/v2/log"
)

// Greeter is a Greeter model.
type EventModel struct {
	ID     int32
	Title  string
	Status bool
}

// GreeterRepo is a Greater repo.
type EventRepo interface {
	Save(context.Context, *EventModel) (*EventModel, error)
	Update(context.Context, *EventModel) (*EventModel, error)
	Delete(context.Context, *EventModel) error
	// FindByID(context.Context, int64) (*EventModel, error)
	// ListByHello(context.Context, string) ([]*EventModel, error)
	ListAll(context.Context) ([]*EventModel, error)
}

// GreeterUsecase is a Greeter usecase.
type EventUsecase struct {
	repo EventRepo
	log  *log.Helper
}

// NewGreeterUsecase new a Greeter usecase.
func NewEventUsecase(repo EventRepo, logger log.Logger) *EventUsecase {
	return &EventUsecase{repo: repo, log: log.NewHelper(logger)}
}

// CreateGreeter creates a Greeter, and returns the new Greeter.
func (uc *EventUsecase) CreateEventModel(ctx context.Context, g *EventModel) (*EventModel, error) {
	uc.log.WithContext(ctx).Infof("biz: CreateEventModel: %#v", g)
	return uc.repo.Save(ctx, g)
}

// UpdateGreeter updates a Greeter, and returns the new Greeter.
func (uc *EventUsecase) UpdateEventModel(ctx context.Context, g *EventModel) (*EventModel, error) {
	uc.log.WithContext(ctx).Infof("biz: UpdateEventModel: %#v", g)
	return uc.repo.Update(ctx, g)
}

// DeleteGreeter deletes a Greeter, and returns the deleted Greeter.
func (uc *EventUsecase) DeleteEventModel(ctx context.Context, g *EventModel) error {
	uc.log.WithContext(ctx).Infof("biz: DeleteEventModel: %#v", g)
	return uc.repo.Delete(ctx, g)
}

// ListAllGreeter lists all Greeters.
func (uc *EventUsecase) ListAll(ctx context.Context) ([]*EventModel, error) {
	uc.log.WithContext(ctx).Infof("biz: ListAll")
	return uc.repo.ListAll(ctx)
}
