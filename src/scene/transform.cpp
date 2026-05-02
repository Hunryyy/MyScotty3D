
#include "transform.h"

Mat4 Transform::local_to_parent() const
{
	return Mat4::translate(translation) * rotation.to_mat() * Mat4::scale(scale);
}

Mat4 Transform::parent_to_local() const
{
	return Mat4::scale(1.0f / scale) * rotation.inverse().to_mat() * Mat4::translate(-translation);
}

Mat4 Transform::local_to_world() const
{
	// A1T1: local_to_world
	// don't use Mat4::inverse() in your code.
	Mat4 l2p = local_to_parent();

	// 检查是否有父节点
	if (auto p = parent.lock())
	{
		// 如果有父节点，递归调用父节点的 local_to_world
		return p->local_to_world() * l2p;
	}

	// 如果没有父节点，当前节点就是根，局部到父节点即是局部到世界
	return l2p;
}

Mat4 Transform::world_to_local() const
{
	// A1T1: world_to_local
	// don't use Mat4::inverse() in your code.
	Mat4 p2l = parent_to_local();

	// 检查是否有父节点
	if (auto p = parent.lock())
	{
		// 先应用世界到父节点的变换，再应用父节点到局部的变换
		// 注意矩阵乘法顺序：后发生的变换写在左边 (p2l * world_to_parent)
		return p2l * p->world_to_local();
	}

	// 如果没有父节点，父节点到局部即是世界到局部
	return p2l;
}

bool operator!=(const Transform &a, const Transform &b)
{
	return a.parent.lock() != b.parent.lock() || a.translation != b.translation ||
		   a.rotation != b.rotation || a.scale != b.scale;
}
