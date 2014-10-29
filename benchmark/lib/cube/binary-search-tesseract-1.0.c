/*
	Copyright 2014 Gregorius van den Hoven - gregoriusvandenhoven@gmail.com
*/

/*
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
	Binary Search Tesseract v1.0
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BSC_M 64

#define BSC_X_MAX 128
#define BSC_Y_MAX 64
#define BSC_Z_MAX 32

#define BSC_X_MIN 32
#define BSC_Y_MIN 16
#define BSC_Z_MIN 8

struct cube
{
	int *w_floor;
	struct w_node **w_axis;
	unsigned char *x_size;
	int *w_volume;
	int volume;
	short w_size;
};

struct w_node
{
	int x_floor[BSC_X_MAX];
	struct x_node *x_axis[BSC_X_MAX];
	unsigned char y_size[BSC_X_MAX];
	short x_volume[BSC_X_MAX];
};

struct x_node
{
	int y_floor[BSC_Y_MAX];
	struct y_node *y_axis[BSC_Y_MAX];
	unsigned char z_size[BSC_Y_MAX];
};

struct y_node
{
	int z_keys[BSC_Z_MAX];
	void *z_vals[BSC_Z_MAX];
};

void *find_key(struct cube *cube, int key, short *w, short *x, short *y, short *z);

void split_w_node(struct cube *cube, short w);
void merge_w_node(struct cube *cube, short w1, short w2);

void split_x_node(struct cube *cube, short w, short x);
void merge_x_node(struct cube *cube, short w, short x1, short x2);

void split_y_node(struct cube *cube, short w, short x, short y);
void merge_y_node(struct cube *cube, short w, short x, short y1, short y2);

void insert_z_node(struct cube *cube, short w, short x, short y, short z, int key, void *val);
void *remove_z_node(struct cube *cube, short w, short x, short y, short z);

inline void *find_index(struct cube *cube, int index, short *w_index, short *x_index, short *y_index, short *z_index)
{
	struct w_node *w_node;
	struct x_node *x_node;
	struct y_node *y_node;
	register short w, x, y;
	int total;

	if (index < 0 || index >= cube->volume)
	{
		return NULL;
	}

	if (index < cube->volume / 2)
	{
		total = 0;

		for (w = 0 ; w < cube->w_size ; w++)
		{
			w_node = cube->w_axis[w];

			if (total + cube->w_volume[w] > index)
			{
				if (index > total + cube->w_volume[w] / 2)
				{
					total += cube->w_volume[w];

					goto backward_x;
				}
				forward_x:

				for (x = 0 ; x < cube->x_size[w] ; x++)
				{
					x_node = w_node->x_axis[x];

					if (total + w_node->x_volume[x] > index)
					{
						if (index > total + w_node->x_volume[x] / 2)
						{
							total += w_node->x_volume[x];

							goto backward_y;
						}
						forward_y:

						for (y = 0 ; y < w_node->y_size[x] ; y++)
						{
							y_node = x_node->y_axis[y];

							if (total + x_node->z_size[y] > index)
							{
								*w_index = w;
								*x_index = x;
								*y_index = y;
								*z_index = index - total;

								return y_node->z_vals[index - total];
							}
							total += x_node->z_size[y];
						}
					}
					total += w_node->x_volume[x];
				}
			}
			total += cube->w_volume[w];
		}
	}
	else
	{
		total = cube->volume;

		for (w = cube->w_size - 1 ; w >= 0 ; w--)
		{
			w_node = cube->w_axis[w];

			if (total - cube->w_volume[w] <= index)
			{
				if (index < total - cube->w_volume[w] / 2)
				{
					total -= cube->w_volume[w];

					goto forward_x;
				}
				backward_x:

				for (x = cube->x_size[w] - 1 ; x >= 0 ; x--)
				{
					x_node = w_node->x_axis[x];

					if (total - w_node->x_volume[x] <= index)
					{
						if (index < total - w_node->x_volume[x] / 2)
						{
							total -= w_node->x_volume[x];

							goto forward_y;
						}
						backward_y:

						for (y = w_node->y_size[x] - 1 ; y >= 0 ; y--)
						{
							y_node = x_node->y_axis[y];

							if (total - x_node->z_size[y] <= index)
							{
								*w_index = w;
								*x_index = x;
								*y_index = y;
								*z_index = x_node->z_size[y] - (total - index);

								return y_node->z_vals[x_node->z_size[y] - (total - index)];
							}
							total -= x_node->z_size[y];
						}
					}
					total -= w_node->x_volume[x];
				}
			}
			total -= cube->w_volume[w];
		}
	}
	return NULL;
}


void *get_key(struct cube *cube, int key)
{
	short w, x, y, z;

	return find_key(cube, key, &w, &x, &y, &z);
}

void *del_key(struct cube *cube, int key)
{
	short w, x, y, z;

	if (find_key(cube, key, &w, &x, &y, &z))
	{
		return remove_z_node(cube, w, x, y, z);
	}
	return NULL;
}

void *get_index(struct cube *cube, int index)
{
	short w, x, y, z;

	return find_index(cube, index, &w, &x, &y, &z);
}

void *del_index(struct cube *cube, int index)
{
	short w, x, y, z;

	if (find_index(cube, index, &w, &x, &y, &z))
	{
		return remove_z_node(cube, w, x, y, z);
	}
	return NULL;
}

struct cube *create_cube(void)
{
	struct cube *cube;

	cube = (struct cube *) calloc(1, sizeof(struct cube));

	return cube;
}

void destroy_cube(struct cube *cube)
{
	if (cube->w_size)
	{
		struct w_node *w_node;
		struct x_node *x_node;
		struct y_node *y_node;

		register short w, x, y;

		for (w = cube->w_size - 1 ; w >= 0 ; w--)
		{
			w_node = cube->w_axis[w];

			for (x = cube->x_size[w] - 1 ; x >= 0 ; x--)
			{
				x_node = w_node->x_axis[x];
			
				for (y = w_node->y_size[x] - 1 ; y >= 0 ; y--)
				{
					y_node = x_node->y_axis[y];
				
					free(y_node);
				}
				free(x_node);
			}
			free(w_node);
		}
		free(cube->w_floor);
		free(cube->w_axis);
		free(cube->w_volume);
		free(cube->x_size);
	}
	free(cube);
}

void set_key(struct cube *cube, int key, void *val)
{
	struct w_node *w_node;
	struct x_node *x_node;
	struct y_node *y_node;

	short mid, w, x, y, z;

	if (cube->w_size == 0)
	{
		cube->w_floor = (int *) malloc(BSC_M * sizeof(int));
		cube->w_axis = (struct w_node **) malloc(BSC_M * sizeof(struct w_node *));
		cube->w_volume = (int *) malloc(BSC_M * sizeof(int));
		cube->x_size = (unsigned char *) malloc(BSC_M * sizeof(unsigned char));

		w_node = cube->w_axis[0] = (struct w_node *) malloc(sizeof(struct w_node));

		x_node = w_node->x_axis[0] = (struct x_node *) malloc(sizeof(struct x_node));

		y_node = x_node->y_axis[0] = (struct y_node *) malloc(sizeof(struct y_node));

		x_node->z_size[0] = 0;

		cube->w_size = cube->x_size[0] = w_node->y_size[0] = 1;
		cube->volume = cube->w_volume[0] = w_node->x_volume[0] = 0;

		w = x = y = z = 0;
		
		cube->w_floor[0] = w_node->x_floor[0] = x_node->y_floor[0] = key;

		goto insert;
	}

	if (key < cube->w_floor[0])
	{
		w_node = cube->w_axis[0];
		x_node = w_node->x_axis[0];
		y_node = x_node->y_axis[0];

		w = x = y = z = 0;

		cube->w_floor[0] = w_node->x_floor[0] = x_node->y_floor[0] = key;

		goto insert;
	}

	// w

	mid = w = cube->w_size - 1;

	while (mid > 3)
	{
		mid /= 2;

		if (key < cube->w_floor[w - mid]) w -= mid;
	}
	while (key < cube->w_floor[w]) --w;

	w_node = cube->w_axis[w];

	// x

	mid = x = cube->x_size[w] - 1;

	while (mid > 3)
	{
		mid /= 2;

		if (key < w_node->x_floor[x - mid]) x -= mid;
	}
	while (key < w_node->x_floor[x]) --x;

	x_node = w_node->x_axis[x];

	// y

	mid = y = w_node->y_size[x] - 1;

	while (mid > 7)
	{
		mid /= 4;

		if (key < x_node->y_floor[y - mid])
		{
			y -= mid;
			if (key < x_node->y_floor[y - mid])
			{
				y -= mid;
				if (key < x_node->y_floor[y - mid])
				{
					y -= mid;
				}
			}
		}
	}
	while (key < x_node->y_floor[y]) --y;

	y_node = x_node->y_axis[y];

	// z

	mid = z = x_node->z_size[y] - 1;

	while (mid > 7)
	{
		mid /= 4;

		if (key < y_node->z_keys[z - mid])
		{
			z -= mid;
			if (key < y_node->z_keys[z - mid])
			{
				z -= mid;
				if (key < y_node->z_keys[z - mid])
				{
					z -= mid;
				}
			}
		}
	}
	while (key < y_node->z_keys[z]) --z;

	if (key == y_node->z_keys[z])
	{
		y_node->z_vals[z] = val;

		return;
	}

	++z;

	insert:

	++cube->volume;
	++cube->w_volume[w];
	++w_node->x_volume[x];

	++x_node->z_size[y];

	if (z + 1 != x_node->z_size[y])
	{
		memmove(&y_node->z_keys[z + 1], &y_node->z_keys[z], (x_node->z_size[y] - z - 1) * sizeof(int));
		memmove(&y_node->z_vals[z + 1], &y_node->z_vals[z], (x_node->z_size[y] - z - 1) * sizeof(void *));
	}

	y_node->z_keys[z] = key;
	y_node->z_vals[z] = val;

	if (x_node->z_size[y] == BSC_Z_MAX)
	{
		split_y_node(cube, w, x, y);

		if (w_node->y_size[x] == BSC_Y_MAX)
		{
			split_x_node(cube, w, x);

			if (cube->x_size[w] == BSC_X_MAX)
			{
				split_w_node(cube, w);
			}
		}
	}
}

inline void *find_key(struct cube *cube, int key, short *w_index, short *x_index, short *y_index, short *z_index)
{
	struct w_node *w_node;
	struct x_node *x_node;
	struct y_node *y_node;

	short mid, w, x, y, z;

	if (cube->w_size == 0 || key < cube->w_floor[0])
	{
		*w_index = *x_index = *y_index = *z_index = 0;

		return NULL;
	}

	// w

	mid = w = cube->w_size - 1;

	while (mid > 3)
	{
		mid /= 2;

		if (key < cube->w_floor[w - mid]) w -= mid;
	}
	while (key < cube->w_floor[w]) --w;

	w_node = cube->w_axis[w];

	// x

	mid = x = cube->x_size[w] - 1;

	while (mid > 3)
	{
		mid /= 2;

		if (key < w_node->x_floor[x - mid]) x -= mid;
	}
	while (key < w_node->x_floor[x]) --x;

	x_node = w_node->x_axis[x];

	// y

	mid = y = w_node->y_size[x] - 1;

	while (mid > 7)
	{
		mid /= 4;

		if (key < x_node->y_floor[y - mid])
		{
			y -= mid;
			if (key < x_node->y_floor[y - mid])
			{
				y -= mid;
				if (key < x_node->y_floor[y - mid])
				{
					y -= mid;
				}
			}
		}
	}
	while (key < x_node->y_floor[y]) --y;

	y_node = x_node->y_axis[y];

	// z

	mid = z = x_node->z_size[y] - 1;

	while (mid > 7)
	{
		mid /= 4;

		if (key < y_node->z_keys[z - mid])
		{
			z -= mid;
			if (key < y_node->z_keys[z - mid])
			{
				z -= mid;
				if (key < y_node->z_keys[z - mid])
				{
					z -= mid;
				}
			}
		}
	}
	while (key < y_node->z_keys[z]) --z;

	*w_index = w;
	*x_index = x;
	*y_index = y;

	if (key == y_node->z_keys[z])
	{
		*z_index = z;

		return y_node->z_vals[z];
	}

	*z_index = z + 1;

	return NULL;
}

inline void insert_w_node(struct cube *cube, short w)
{
	++cube->w_size;

	if (cube->w_size % BSC_M == 0)
	{
		cube->w_floor = (int *) realloc(cube->w_floor, (cube->w_size + BSC_M) * sizeof(int));
		cube->w_axis = (struct w_node **) realloc(cube->w_axis, (cube->w_size + BSC_M) * sizeof(struct w_node *));
		cube->w_volume = (int *) realloc(cube->w_volume, (cube->w_size + BSC_M) * sizeof(int));
		cube->x_size = (unsigned char *) realloc(cube->x_size, (cube->w_size + BSC_M) * sizeof(unsigned char));
	}

	if (w + 1 != cube->w_size)
	{
		memmove(&cube->w_floor[w + 1], &cube->w_floor[w], (cube->w_size - w - 1) * sizeof(int));
		memmove(&cube->w_axis[w + 1], &cube->w_axis[w], (cube->w_size - w - 1) * sizeof(struct w_node *));
		memmove(&cube->w_volume[w + 1], &cube->w_volume[w], (cube->w_size - w - 1) * sizeof(int));
		memmove(&cube->x_size[w + 1], &cube->x_size[w], (cube->w_size - w - 1) * sizeof(unsigned char));
	}

	cube->w_axis[w] = (struct w_node *) malloc(sizeof(struct w_node));
}

void remove_w_node(struct cube *cube, short w)
{
	cube->w_size--;

	free(cube->w_axis[w]);

	if (cube->w_size)
	{
		if (cube->w_size != w)
		{
			memmove(&cube->w_floor[w], &cube->w_floor[w + 1], (cube->w_size - w) * sizeof(int));
			memmove(&cube->w_axis[w], &cube->w_axis[w + 1], (cube->w_size - w) * sizeof(struct w_node *));
			memmove(&cube->w_volume[w], &cube->w_volume[w + 1], (cube->w_size - w) * sizeof(int));
			memmove(&cube->x_size[w], &cube->x_size[w + 1], (cube->w_size - w) * sizeof(unsigned char));
		}
	}
	else
	{
		free(cube->w_floor);
		free(cube->w_axis);
		free(cube->w_volume);
		free(cube->x_size);
	}
}

inline void insert_x_node(struct cube *cube, short w, short x)
{
	struct w_node *w_node = cube->w_axis[w];

	short x_size = ++cube->x_size[w];

	if (x_size != x + 1)
	{
		memmove(&w_node->x_floor[x + 1], &w_node->x_floor[x], (x_size - x - 1) * sizeof(int));
		memmove(&w_node->x_axis[x + 1], &w_node->x_axis[x], (x_size - x - 1) * sizeof(struct x_node *));
		memmove(&w_node->x_volume[x + 1], &w_node->x_volume[x], (x_size - x - 1) * sizeof(short));
		memmove(&w_node->y_size[x + 1], &w_node->y_size[x], (x_size - x - 1) * sizeof(unsigned char));
	}

	w_node->x_axis[x] = (struct x_node *) malloc(sizeof(struct x_node));
}

void remove_x_node(struct cube *cube, short w, short x)
{
	struct w_node *w_node = cube->w_axis[w];

	cube->x_size[w]--;

	free(w_node->x_axis[x]);

	if (cube->x_size[w])
	{
		if (cube->x_size[w] != x)
		{
			memmove(&w_node->x_floor[x], &w_node->x_floor[x + 1], (cube->x_size[w] - x ) * sizeof(int));
			memmove(&w_node->x_axis[x], &w_node->x_axis[x + 1], (cube->x_size[w] - x ) * sizeof(struct x_node *));
			memmove(&w_node->x_volume[x], &w_node->x_volume[x + 1], (cube->x_size[w] - x ) * sizeof(short));
			memmove(&w_node->y_size[x], &w_node->y_size[x + 1], (cube->x_size[w] - x ) * sizeof(unsigned char));
		}

		if (x == 0)
		{
			cube->w_floor[w] = w_node->x_floor[0];
		}
	}
	else
	{
		remove_w_node(cube, w);
	}
}

inline void insert_y_node(struct cube *cube, short w, short x, short y)
{
	struct x_node *x_node = cube->w_axis[w]->x_axis[x];

	short y_size = ++cube->w_axis[w]->y_size[x];

	if (y_size != y + 1)
	{
		memmove(&x_node->y_floor[y + 1], &x_node->y_floor[y], (y_size - y - 1) * sizeof(int));
		memmove(&x_node->y_axis[y + 1], &x_node->y_axis[y], (y_size - y - 1) * sizeof(struct y_node *));
		memmove(&x_node->z_size[y + 1], &x_node->z_size[y], (y_size - y - 1) * sizeof(unsigned char));
	}

	x_node->y_axis[y] = (struct y_node *) malloc(sizeof(struct y_node));
}

void remove_y_node(struct cube *cube, short w, short x, short y)
{
	struct w_node *w_node = cube->w_axis[w];
	struct x_node *x_node = w_node->x_axis[x];

	w_node->y_size[x]--;

	free(x_node->y_axis[y]);

	if (w_node->y_size[x])
	{
		if (w_node->y_size[x] != y)
		{

			memmove(&x_node->y_floor[y], &x_node->y_floor[y + 1], (w_node->y_size[x] - y ) * sizeof(int));
			memmove(&x_node->y_axis[y], &x_node->y_axis[y + 1], (w_node->y_size[x] - y ) * sizeof(struct y_node *));
			memmove(&x_node->z_size[y], &x_node->z_size[y + 1], (w_node->y_size[x] - y ) * sizeof(unsigned char));
		}

		if (y == 0)
		{
			cube->w_axis[w]->x_floor[x] = x_node->y_floor[0];

			if (x == 0)
			{
				cube->w_floor[w] = x_node->y_floor[0];
			}
		}
	}
	else
	{
		remove_x_node(cube, w, x);
	}
}

inline void *remove_z_node(struct cube *cube, short w, short x, short y, short z)
{
	struct w_node *w_node = cube->w_axis[w];
	struct x_node *x_node = w_node->x_axis[x];
	struct y_node *y_node = x_node->y_axis[y];
	void *val;

	cube->volume--;

	cube->w_volume[w]--;
	w_node->x_volume[x]--;

	x_node->z_size[y]--;

	val = y_node->z_vals[z];

	if (x_node->z_size[y] != z)
	{
		memmove(&y_node->z_keys[z], &y_node->z_keys[z + 1], (x_node->z_size[y] - z) * sizeof(int));
		memmove(&y_node->z_vals[z], &y_node->z_vals[z + 1], (x_node->z_size[y] - z) * sizeof(void *));
	}

	if (x_node->z_size[y])
	{
		if (z == 0)
		{
			x_node->y_floor[y] = y_node->z_keys[z];

			if (y == 0)
			{
				w_node->x_floor[x] = y_node->z_keys[z];

				if (x == 0)
				{
					cube->w_floor[w] = y_node->z_keys[z];
				}
			}
		}

		if (y && x_node->z_size[y] < BSC_Z_MIN && x_node->z_size[y - 1] < BSC_Z_MIN)
		{
			merge_y_node(cube, w, x, y - 1, y);

			if (x && w_node->y_size[x] < BSC_Y_MIN && w_node->y_size[x - 1] < BSC_Y_MIN)
			{
				merge_x_node(cube, w, x - 1, x);

				if (w && cube->x_size[w] < BSC_X_MIN && cube->x_size[w - 1] < BSC_X_MIN)
				{
					merge_w_node(cube, w - 1, w);
				}
			}
		}
	}
	else
	{
		remove_y_node(cube, w, x, y);
	}
	return val;
}

void split_w_node(struct cube *cube, short w)
{
	struct w_node *w_node1, *w_node2;
	short x;
	int volume;

	insert_w_node(cube, w + 1);

	w_node1 = cube->w_axis[w];
	w_node2 = cube->w_axis[w + 1];

	cube->x_size[w + 1] = cube->x_size[w] / 2;
	cube->x_size[w] -= cube->x_size[w + 1];

	memcpy(&w_node2->x_floor[0], &w_node1->x_floor[cube->x_size[w]], cube->x_size[w + 1] * sizeof(int));
	memcpy(&w_node2->x_axis[0], &w_node1->x_axis[cube->x_size[w]], cube->x_size[w + 1] * sizeof(struct x_node *));
	memcpy(&w_node2->x_volume[0], &w_node1->x_volume[cube->x_size[w]], cube->x_size[w + 1] * sizeof(short));
	memcpy(&w_node2->y_size[0], &w_node1->y_size[cube->x_size[w]], cube->x_size[w + 1] * sizeof(unsigned char));

	for (x = volume = 0 ; x < cube->x_size[w] ; x++)
	{
		volume += w_node1->x_volume[x];
	}

	cube->w_volume[w + 1] = cube->w_volume[w] - volume;
	cube->w_volume[w] = volume;

	cube->w_floor[w + 1] = w_node2->x_floor[0];
}

void merge_w_node(struct cube *cube, short w1, short w2)
{
	struct w_node *w_node1 = cube->w_axis[w1];
	struct w_node *w_node2 = cube->w_axis[w2];

	memcpy(&w_node1->x_floor[cube->x_size[w1]], &w_node2->x_floor[0], cube->x_size[w2] * sizeof(int));
	memcpy(&w_node1->x_axis[cube->x_size[w1]], &w_node2->x_axis[0], cube->x_size[w2] * sizeof(struct x_node *));
	memcpy(&w_node1->x_volume[cube->x_size[w1]], &w_node2->x_volume[0], cube->x_size[w2] * sizeof(short));
	memcpy(&w_node1->y_size[cube->x_size[w1]], &w_node2->y_size[0], cube->x_size[w2] * sizeof(unsigned char));

	cube->x_size[w1] += cube->x_size[w2];

	cube->w_volume[w1] += cube->w_volume[w2];

	remove_w_node(cube, w2);
}

void split_x_node(struct cube *cube, short w, short x)
{
	struct w_node *w_node = cube->w_axis[w];
	struct x_node *x_node1, *x_node2;
	short y;
	int volume;

	insert_x_node(cube, w, x + 1);

	x_node1 = w_node->x_axis[x];
	x_node2 = w_node->x_axis[x + 1];

	w_node->y_size[x + 1] = w_node->y_size[x] / 2;
	w_node->y_size[x] -= w_node->y_size[x + 1];

	memcpy(&x_node2->y_floor[0], &x_node1->y_floor[w_node->y_size[x]], w_node->y_size[x + 1] * sizeof(int));
	memcpy(&x_node2->y_axis[0], &x_node1->y_axis[w_node->y_size[x]], w_node->y_size[x + 1] * sizeof(struct y_node *));
	memcpy(&x_node2->z_size[0], &x_node1->z_size[w_node->y_size[x]], w_node->y_size[x + 1] * sizeof(unsigned char));

	for (y = volume = 0 ; y < w_node->y_size[x] ; y++)
	{
		volume += x_node1->z_size[y];
	}

	w_node->x_volume[x + 1] = w_node->x_volume[x] - volume;
	w_node->x_volume[x] = volume;

	cube->w_axis[w]->x_floor[x + 1] = x_node2->y_floor[0];
}

void merge_x_node(struct cube *cube, short w, short x1, short x2)
{
	struct w_node *w_node = cube->w_axis[w];
	struct x_node *x_node1 = w_node->x_axis[x1];
	struct x_node *x_node2 = w_node->x_axis[x2];

	memcpy(&x_node1->y_floor[w_node->y_size[x1]], &x_node2->y_floor[0], w_node->y_size[x2] * sizeof(int));
	memcpy(&x_node1->y_axis[w_node->y_size[x1]], &x_node2->y_axis[0], w_node->y_size[x2] * sizeof(struct y_node *));
	memcpy(&x_node1->z_size[w_node->y_size[x1]], &x_node2->z_size[0], w_node->y_size[x2] * sizeof(unsigned char));

	w_node->y_size[x1] += w_node->y_size[x2];

	w_node->x_volume[x1] += w_node->x_volume[x2];

	remove_x_node(cube, w, x2);
}

void split_y_node(struct cube *cube, short w, short x, short y)
{
	struct x_node *x_node = cube->w_axis[w]->x_axis[x];
	struct y_node *y_node1, *y_node2;

	insert_y_node(cube, w, x, y + 1);

	y_node1 = x_node->y_axis[y];
	y_node2 = x_node->y_axis[y + 1];

	x_node->z_size[y + 1] = x_node->z_size[y] / 2;
	x_node->z_size[y] -= x_node->z_size[y + 1];

	memcpy(&y_node2->z_keys[0], &y_node1->z_keys[x_node->z_size[y]], x_node->z_size[y + 1] * sizeof(int));
	memcpy(&y_node2->z_vals[0], &y_node1->z_vals[x_node->z_size[y]], x_node->z_size[y + 1] * sizeof(void *));

	x_node->y_floor[y + 1] = y_node2->z_keys[0];
}

void merge_y_node(struct cube *cube, short w, short x, short y1, short y2)
{
	struct x_node *x_node = cube->w_axis[w]->x_axis[x];

	struct y_node *y_node1 = x_node->y_axis[y1];
	struct y_node *y_node2 = x_node->y_axis[y2];

	memcpy(&y_node1->z_keys[x_node->z_size[y1]], &y_node2->z_keys[0], x_node->z_size[y2] * sizeof(int));
	memcpy(&y_node1->z_vals[x_node->z_size[y1]], &y_node2->z_vals[0], x_node->z_size[y2] * sizeof(void *));

	x_node->z_size[y1] += x_node->z_size[y2];

	remove_y_node(cube, w, x, y2);
}

size_t size_cube(struct cube *cube)
{
	struct w_node *w_node;
	short w, x, y;
	size_t size = 0;

	for (w = cube->w_size - 1 ; w >= 0 ; w--)
	{
		w_node = cube->w_axis[w];

		for (x = cube->x_size[w] - 1 ; x >= 0 ; x--)
		{
			for (y = w_node->y_size[x] - 1 ; y >= 0 ; y--)
			{
				size += sizeof(struct y_node);
			}
			size += sizeof(struct x_node);
		}
		size += sizeof(struct w_node);
	}

	size += BSC_M * sizeof(int);
	size += BSC_M * sizeof(struct w_node *);
	size += BSC_M * sizeof(int);
	size += BSC_M * sizeof(unsigned char);
	size += sizeof(struct cube);

	return size;
}

