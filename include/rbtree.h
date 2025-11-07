/**
 * rbtree.h - Linux风格的红黑树
 * 
 * 红黑树是一种自平衡二叉搜索树，Linux内核中广泛使用
 * 用于VMA管理、调度器、文件系统等
 */

#ifndef _RBTREE_H
#define _RBTREE_H

#include <types.h>

/* 红黑树节点颜色 */
#define RB_RED      0
#define RB_BLACK    1

/**
 * 红黑树节点
 */
struct rb_node {
    unsigned long  rb_parent_color;  /* 父节点指针和颜色（最低位存储颜色）*/
    struct rb_node *rb_right;
    struct rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));

/**
 * 红黑树根节点
 */
struct rb_root {
    struct rb_node *rb_node;
};

/**
 * 初始化红黑树根节点
 */
#define RB_ROOT  (struct rb_root) { NULL, }

/**
 * 获取父节点
 */
#define rb_parent(r)   ((struct rb_node *)((r)->rb_parent_color & ~3))

/**
 * 获取节点颜色
 */
#define rb_color(r)   ((r)->rb_parent_color & 1)

/**
 * 检查节点是否为红色
 */
#define rb_is_red(r)   (!rb_color(r))

/**
 * 检查节点是否为黑色
 */
#define rb_is_black(r) rb_color(r)

/**
 * 设置节点为红色
 */
static inline void rb_set_red(struct rb_node *rb)
{
    rb->rb_parent_color &= ~1;
}

/**
 * 设置节点为黑色
 */
static inline void rb_set_black(struct rb_node *rb)
{
    rb->rb_parent_color |= 1;
}

/**
 * 设置父节点
 */
static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)
{
    rb->rb_parent_color = (rb->rb_parent_color & 3) | (unsigned long)p;
}

/**
 * 设置父节点和颜色
 */
static inline void rb_set_parent_color(struct rb_node *rb,
                                       struct rb_node *p, int color)
{
    rb->rb_parent_color = (unsigned long)p | color;
}

/**
 * 初始化红黑树节点
 */
static inline void rb_init_node(struct rb_node *rb)
{
    rb->rb_parent_color = 0;
    rb->rb_left = NULL;
    rb->rb_right = NULL;
}

/**
 * 检查节点是否为空
 */
static inline int rb_empty_node(const struct rb_node *node)
{
    return node->rb_parent_color == (unsigned long)node;
}

/**
 * 清空节点
 */
static inline void rb_clear_node(struct rb_node *node)
{
    node->rb_parent_color = (unsigned long)node;
}

/**
 * 检查树是否为空
 */
static inline int rb_empty_root(const struct rb_root *root)
{
    return root->rb_node == NULL;
}

/**
 * 获取包含红黑树节点的结构体指针
 */
#define rb_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/**
 * 获取第一个节点（最小值）
 */
extern struct rb_node *rb_first(const struct rb_root *root);

/**
 * 获取最后一个节点（最大值）
 */
extern struct rb_node *rb_last(const struct rb_root *root);

/**
 * 获取下一个节点
 */
extern struct rb_node *rb_next(const struct rb_node *node);

/**
 * 获取前一个节点
 */
extern struct rb_node *rb_prev(const struct rb_node *node);

/**
 * 插入节点（需要调用者先找到插入位置）
 */
extern void rb_insert_color(struct rb_node *node, struct rb_root *root);

/**
 * 删除节点
 */
extern void rb_erase(struct rb_node *node, struct rb_root *root);

/**
 * 替换节点
 */
extern void rb_replace_node(struct rb_node *victim, struct rb_node *new,
                            struct rb_root *root);

/**
 * 链接新节点到父节点
 */
static inline void rb_link_node(struct rb_node *node, struct rb_node *parent,
                                struct rb_node **rb_link)
{
    node->rb_parent_color = (unsigned long)parent;
    node->rb_left = node->rb_right = NULL;
    *rb_link = node;
}

/**
 * 遍历红黑树（中序遍历）
 */
#define rb_for_each(pos, root) \
    for (pos = rb_first(root); pos; pos = rb_next(pos))

/**
 * 遍历红黑树中的每个元素
 */
#define rb_for_each_entry(pos, root, member) \
    for (pos = rb_entry(rb_first(root), typeof(*pos), member); \
         &pos->member; \
         pos = rb_entry(rb_next(&pos->member), typeof(*pos), member))

/**
 * 安全遍历红黑树中的每个元素
 */
#define rb_for_each_entry_safe(pos, n, root, member) \
    for (pos = rb_entry(rb_first(root), typeof(*pos), member); \
         &pos->member && ({ n = rb_entry(rb_next(&pos->member), \
                                         typeof(*pos), member); 1; }); \
         pos = n)

/* 清除节点（设置为未链接状态）*/
#define RB_CLEAR_NODE(node) \
    ((node)->__rb_parent_color = (unsigned long)(node))

/* 检查节点是否为空（未链接）*/
#define RB_EMPTY_NODE(node) \
    ((node)->__rb_parent_color == (unsigned long)(node))

#endif /* _RBTREE_H */

