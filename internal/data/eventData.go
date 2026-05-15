package data

import (
	"context"
	"fmt"

	"bubble/internal/biz"

	"github.com/go-kratos/kratos/v2/log"
)

type bubbleRepo struct {
	data *Data
	log  *log.Helper
}

// NewGreeterRepo .
func NewBubbleRepo(data *Data, logger log.Logger) biz.EventRepo {
	return &bubbleRepo{
		data: data,
		log:  log.NewHelper(logger),
	}
}

func (r *bubbleRepo) Save(ctx context.Context, g *biz.EventModel) (*biz.EventModel, error) {
	fmt.Println("data: Save", g)
	return g, nil
}

func (r *bubbleRepo) Update(ctx context.Context, g *biz.EventModel) (*biz.EventModel, error) {
	fmt.Println("data: Update", g)
	return g, nil
}

func (r *bubbleRepo) Delete(ctx context.Context, g *biz.EventModel) error {
	fmt.Println("data: Delete", g)
	return nil
}

// func (r *bubbleRepo) FindByID(context.Context, int64) (*biz.EventModel, error) {
// 	return nil, nil
// }

// func (r *greeterRepo) ListByHello(context.Context, string) ([]*biz.EventModel, error) {
// 	return nil, nil
// }

func (r *bubbleRepo) ListAll(context.Context) ([]*biz.EventModel, error) {
	fmt.Println("data: ListAll")
	return nil, nil
}
