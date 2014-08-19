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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#define BSC_X 64
#define BSC_Y 32
#define BSC_Z 8

#define BSC_L 3

struct cube
{
	int volume;
	short w_size;
	int *w_floor;
	struct w_node **w_axis;
};

struct w_node
{
	int volume;
	short x_size;
	int *x_floor;
	struct x_node **x_axis;
};

struct x_node
{
	int volume;
	short y_size;
	int *y_floor;
	struct y_node **y_axis;
};

struct y_node
{
	short z_size;
	struct z_node *z_axis;
};

struct z_node
{
	int key;
	void* val;
};

struct z_node *find_key(struct cube *cube, int key, short *w_index, short *x_index, short *y_index, short *z_index);

void split_w_node(struct cube *cube, short w_index);
void merge_w_node(struct cube *cube, short w_index1, short w_index2);

void split_x_node(struct cube *cube, short w_index, short x_index);
void merge_x_node(struct cube *cube, short w_index, short x_index1, short x_index2);

void split_y_node(struct cube *cube, short w_index, short x_index, short y_index);
void merge_y_node(struct cube *cube, short w_index, short x_index, short y_index1, short y_index2);

void insert_z_node(struct cube *cube, short w_index, short x_index, short y_index, short z_index, int key, void *val);
void remove_z_node(struct cube *cube, short w_index, short x_index, short y_index, short z_index);

void deflate_cube(struct cube *cube);
void deflate_w_axis(struct cube *cube, short w_index);
void deflate_x_axis(struct cube *cube, short w_index, short x_index);

struct z_node *find_index(struct cube *cube, int index, short *w_index, short *x_index, short *y_index, short *z_index)
{
	struct w_node *w_node;
	struct x_node *x_node;
	struct y_node *y_node;
	register short w, x, y;
	int total;

	if (index <= 0 || index >= cube->volume)
	{
		if (cube->volume && index == 0)
		{
			*w_index = *x_index = *y_index = *z_index = 0;

			return &cube->w_axis[0]->x_axis[0]->y_axis[0]->z_axis[0];
		}
		return NULL;
	}

	if (index < cube->volume / 2)
	{
		total = 0;

		for (w = 0 ; w < cube->w_size ; w++)
		{
			w_node = cube->w_axis[w];

			if (total + w_node->volume > index)
			{
				if (index > total + w_node->volume / 2)
				{
					total += w_node->volume;

					goto backward_x;
				}
				forward_x:

				for (x = 0 ; x < w_node->x_size ; x++)
				{
					x_node = w_node->x_axis[x];

					if (total + x_node->volume > index)
					{
						if (index > total + x_node->volume / 2)
						{
							total += x_node->volume;

							goto backward_y;
						}
						forward_y:

						for (y = 0 ; y < x_node->y_size ; y++)
						{
							y_node = x_node->y_axis[y];

							if (total + y_node->z_size > index)
							{
								*w_index = w;
								*x_index = x;
								*y_index = y;
								*z_index = index - total;

								return &y_node->z_axis[index - total];
							}
							total += y_node->z_size;
						}
					}
					total += x_node->volume;
				}
			}
			total += w_node->volume;
		}
	}
	else
	{
		total = cube->volume;

		for (w = cube->w_size - 1 ; w >= 0 ; w--)
		{
			w_node = cube->w_axis[w];

			if (total - w_node->volume <= index)
			{
				if (index < total - w_node->volume / 2)
				{
					total -= w_node->volume;

					goto forward_x;
				}
				backward_x:

				for (x = w_node->x_size - 1 ; x >= 0 ; x--)
				{
					x_node = w_node->x_axis[x];

					if (total - x_node->volume <= index)
					{
						if (index < total - x_node->volume / 2)
						{
							total -= x_node->volume;

							goto forward_y;
						}
						backward_y:

						for (y = x_node->y_size - 1 ; y >= 0 ; y--)
						{
							y_node = x_node->y_axis[y];

							if (total - y_node->z_size <= index)
							{
								*w_index = w;
								*x_index = x;
								*y_index = y;
								*z_index = y_node->z_size - (total - index);

								return &y_node->z_axis[y_node->z_size - (total - index)];
							}
							total -= y_node->z_size;
						}
					}
					total -= x_node->volume;
				}
			}
			total -= w_node->volume;
		}
	}
	return NULL;
}

void set_key(struct cube *cube, int key, void *val)
{
	struct z_node *z_node;
	short w_index, x_index, y_index, z_index;

	z_node = find_key(cube, key, &w_index, &x_index, &y_index, &z_index);

	if (z_node)
	{
		z_node->val = val;
	}
	else
	{
		insert_z_node(cube, w_index, x_index, y_index, z_index, key, val);
	}
}

struct z_node *get_key(struct cube *cube, int key)
{
	short w_index, x_index, y_index, z_index;

	return find_key(cube, key, &w_index, &x_index, &y_index, &z_index);
}

void del_key(struct cube *cube, int key)
{
	short w_index, x_index, y_index, z_index;

	if (find_key(cube, key, &w_index, &x_index, &y_index, &z_index))
	{
		remove_z_node(cube, w_index, x_index, y_index, z_index);
	}
}

struct z_node *get_index(struct cube *cube, int index)
{
	short w_index, x_index, y_index, z_index;

	return find_index(cube, index, &w_index, &x_index, &y_index, &z_index);
}

void del_index(struct cube *cube, int index)
{
	short w_index, x_index, y_index, z_index;

	if (find_index(cube, index, &w_index, &x_index, &y_index, &z_index))
	{
		remove_z_node(cube, w_index, x_index, y_index, z_index);
	}
}

struct cube *create_cube(void)
{
	struct cube *cube;

	cube = (struct cube *) calloc(1, sizeof(struct cube));

	return cube;
}

void destroy_cube(struct cube *cube)
{
	if (cube->volume)
	{
		struct w_node *w_node;
		struct x_node *x_node;
		struct y_node *y_node;

		register short w, x, y, z;

		for (w = cube->w_size - 1 ; w >= 0 ; w--)
		{
			w_node = cube->w_axis[w];

			for (x = w_node->x_size - 1 ; x >= 0 ; x--)
			{
				x_node = w_node->x_axis[x];
			
				for (y = x_node->y_size - 1 ; y >= 0 ; y--)
				{
					y_node = x_node->y_axis[y];
				
					free(y_node->z_axis);
					free(y_node);
				}
				free(x_node->y_floor);
				free(x_node->y_axis);
				free(x_node);
			}
			free(w_node->x_floor);
			free(w_node->x_axis);
			free(w_node);
		}
		free(cube->w_floor);
		free(cube->w_axis);
	}
	free(cube);
}

struct z_node *find_key(struct cube *cube, int key, short *w_index, short *x_index, short *y_index, short *z_index)
{
	struct w_node *w_node;
	struct x_node *x_node;
	struct y_node *y_node;

	register short bot, mid, w, x, y, z;

	if (cube->volume == 0 || key < cube->w_floor[0])
	{
		*w_index = *x_index = *y_index = *z_index = 0;

		return NULL;
	}

	bot = 0;
	mid = w = cube->w_size - 1;

	while (1)
	{
		if (key < cube->w_floor[mid])
		{
			w = mid - 1;
		}
		else
		{
			bot = mid;
		}

		if (bot + BSC_L > w)
		{
			break;
		}
		mid = w - (w - bot) / 2;
	}

	while (key < cube->w_floor[w]) w--;

	w_node = cube->w_axis[w];

	bot = 0;
	mid = x = w_node->x_size - 1;

	while (1)
	{
		if (key < w_node->x_floor[mid])
		{
			x = mid - 1;
		}
		else
		{
			bot = mid;
		}

		if (bot + BSC_L > x)
		{
			break;
		}
		mid = x - (x - bot) / 2;
	}

	while (key < w_node->x_floor[x]) x--;

	x_node = w_node->x_axis[x];

	bot = 0;
	mid = y = x_node->y_size - 1;

	while (1)
	{
		if (key < x_node->y_floor[mid])
		{
			y = mid - 1;
		}
		else
		{
			bot = mid;
		}

		if (bot + BSC_L > y)
		{
			break;
		}
		mid = y - (y - bot) / 2;
	}

	while (key < x_node->y_floor[y]) y--;

	y_node = x_node->y_axis[y];

	bot = 0;
	mid = z = y_node->z_size - 1;

	while (1)
	{
		if (key < y_node->z_axis[mid].key)
		{
			z = mid - 1;
		}
		else
		{
			bot = mid;
		}

		if (bot + BSC_L > z)
		{
			break;
		}

		mid = z - (z - bot) / 2;
	}

	while (key < y_node->z_axis[z].key) z--;

	if (key == y_node->z_axis[z].key)
	{

		*w_index = w;
		*x_index = x;
		*y_index = y;
		*z_index = z;

		return &y_node->z_axis[z];
	}

	*w_index = w;
	*x_index = x;
	*y_index = y;
	*z_index = z + 1;

	return NULL;
}

void insert_w_node(struct cube *cube, short w_index)
{
	cube->w_size++;

	cube->w_floor = (int *) realloc(cube->w_floor, cube->w_size * sizeof(int));
	cube->w_axis = (struct w_node **) realloc(cube->w_axis, cube->w_size * sizeof(struct w_node *));

	if (w_index + 1 != cube->w_size)
	{
		memmove(&cube->w_floor[w_index + 1], &cube->w_floor[w_index], (cube->w_size - w_index - 1) * sizeof(int));
		memmove(&cube->w_axis[w_index + 1], &cube->w_axis[w_index], (cube->w_size - w_index - 1) * sizeof(struct w_node *));
	}

	cube->w_axis[w_index] = (struct w_node *) calloc(1, sizeof(struct w_node));
}

void remove_w_node(struct cube *cube, short w_index)
{
	cube->w_size--;

	free(cube->w_axis[w_index]->x_floor);
	free(cube->w_axis[w_index]->x_axis);
	free(cube->w_axis[w_index]);

	if (cube->w_size)
	{
		if (cube->w_size != w_index)
		{
			memmove(&cube->w_floor[w_index], &cube->w_floor[w_index + 1], (cube->w_size - w_index) * sizeof(int));			
			memmove(&cube->w_axis[w_index], &cube->w_axis[w_index + 1], (cube->w_size - w_index) * sizeof(struct w_node *));
		}

		cube->w_floor = (int *) realloc(cube->w_floor, cube->w_size * sizeof(int));
		cube->w_axis = (struct w_node **) realloc(cube->w_axis, cube->w_size * sizeof(struct w_node *));
	}
	else
	{
		free(cube->w_axis);
	}
}

void insert_x_node(struct cube *cube, short w_index, short x_index)
{
	struct w_node *w_node = cube->w_axis[w_index];

	w_node->x_size++;

	w_node->x_floor = (int *) realloc(w_node->x_floor, w_node->x_size * sizeof(int));
	w_node->x_axis = (struct x_node **) realloc(w_node->x_axis, w_node->x_size * sizeof(struct x_node *));

	if (w_node->x_size != x_index + 1)
	{
		memmove(&w_node->x_floor[x_index + 1], &w_node->x_floor[x_index], (w_node->x_size - x_index - 1) * sizeof(int));
		memmove(&w_node->x_axis[x_index + 1], &w_node->x_axis[x_index], (w_node->x_size - x_index - 1) * sizeof(struct x_node *));
	}

	w_node->x_axis[x_index] = (struct x_node *) calloc(1, sizeof(struct x_node));
}

void remove_x_node(struct cube *cube, short w_index, short x_index)
{
	struct w_node *w_node = cube->w_axis[w_index];

	w_node->x_size--;

	free(w_node->x_axis[x_index]->y_floor);
	free(w_node->x_axis[x_index]->y_axis);

	free(w_node->x_axis[x_index]);

	if (w_node->x_size)
	{
		if (w_node->x_size != x_index)
		{
			memmove(&w_node->x_floor[x_index], &w_node->x_floor[x_index + 1], (w_node->x_size - x_index ) * sizeof(int));
			memmove(&w_node->x_axis[x_index], &w_node->x_axis[x_index + 1], (w_node->x_size - x_index ) * sizeof(struct x_node *));
		}

		w_node->x_floor = (int *) realloc(w_node->x_floor, w_node->x_size * sizeof(int));
		w_node->x_axis = (struct x_node **) realloc(w_node->x_axis, w_node->x_size * sizeof(struct x_node *));

		if (x_index == 0)
		{
			cube->w_floor[w_index] = w_node->x_floor[0];
		}
	}
	else
	{
		remove_w_node(cube, w_index);
	}
}

void insert_y_node(struct cube *cube, short w_index, short x_index, short y_index)
{
	struct x_node *x_node = cube->w_axis[w_index]->x_axis[x_index];

	x_node->y_size++;

	x_node->y_floor = (int *) realloc(x_node->y_floor, x_node->y_size * sizeof(int));
	x_node->y_axis  = (struct y_node **) realloc(x_node->y_axis, x_node->y_size * sizeof(struct y_node *));

	if (x_node->y_size != y_index + 1)
	{
		memmove(&x_node->y_floor[y_index + 1], &x_node->y_floor[y_index], (x_node->y_size - y_index - 1) * sizeof(int));
		memmove(&x_node->y_axis[y_index + 1], &x_node->y_axis[y_index], (x_node->y_size - y_index - 1) * sizeof(struct y_node *));
	}

	x_node->y_axis[y_index] = (struct y_node *) calloc(1, sizeof(struct y_node));
}

void remove_y_node(struct cube *cube, short w_index, short x_index, short y_index)
{
	struct x_node *x_node = cube->w_axis[w_index]->x_axis[x_index];

	x_node->y_size--;

	free(x_node->y_axis[y_index]->z_axis);

	free(x_node->y_axis[y_index]);

	if (x_node->y_size)
	{
		if (x_node->y_size != y_index)
		{

			memmove(&x_node->y_floor[y_index], &x_node->y_floor[y_index + 1], (x_node->y_size - y_index ) * sizeof(int));
			memmove(&x_node->y_axis[y_index], &x_node->y_axis[y_index + 1], (x_node->y_size - y_index ) * sizeof(struct y_node *));
		}
		x_node->y_floor = (int *) realloc(x_node->y_floor, x_node->y_size * sizeof(int));
		x_node->y_axis = (struct y_node **) realloc(x_node->y_axis, x_node->y_size * sizeof(struct y_node *));

		if (y_index == 0)
		{
			cube->w_axis[w_index]->x_floor[x_index] = x_node->y_floor[0];

			if (x_index == 0)
			{
				cube->w_floor[w_index] = x_node->y_floor[0];
			}
		}
	}
	else
	{
		remove_x_node(cube, w_index, x_index);
	}
}

void insert_z_node(struct cube *cube, short w_index, short x_index, short y_index, short z_index, int key, void *val)
{
	struct w_node *w_node;
	struct x_node *x_node;
	struct y_node *y_node;
	struct z_node *z_node;

	if (cube->volume == 0)
	{
		cube->w_floor = (int *) calloc(1, sizeof(int));
		cube->w_axis = (struct w_node **) calloc(1, sizeof(struct w_node *));

		w_node = cube->w_axis[0] = (struct w_node *) calloc(1, sizeof(struct w_node));

		w_node->x_floor = (int *) calloc(1, sizeof(int));
		w_node->x_axis = (struct x_node **) calloc(1, sizeof(struct x_node *));

		x_node = w_node->x_axis[0] = (struct x_node *) calloc(1, sizeof(struct x_node));

		x_node->y_floor = (int *) calloc(1, sizeof(int));
		x_node->y_axis = (struct y_node **) calloc(1, sizeof(struct y_node *));

		y_node = x_node->y_axis[0] = (struct y_node *) calloc(1, sizeof(struct y_node));

		cube->w_size = w_node->x_size = x_node->y_size = 1;
	}
	else
	{
		w_node = cube->w_axis[w_index];
		x_node = w_node->x_axis[x_index];
		y_node = x_node->y_axis[y_index];
	}

	cube->volume++;
	w_node->volume++;
	x_node->volume++;

	y_node->z_size++;

	y_node->z_axis = (struct z_node *) realloc(y_node->z_axis, y_node->z_size * sizeof(struct z_node));

	if (z_index + 1 != y_node->z_size)
	{
		memmove(&y_node->z_axis[z_index + 1], &y_node->z_axis[z_index], (y_node->z_size - z_index - 1) * sizeof(struct z_node));
	}

	z_node = &y_node->z_axis[z_index];

	z_node->key = key;
	z_node->val = val;

	if (z_index == 0)
	{
		cube->w_floor[0] = w_node->x_floor[0] = x_node->y_floor[0] = z_node->key;
	}

	if (y_node->z_size >= BSC_Z * 4)
	{
		split_y_node(cube, w_index, x_index, y_index);

		if (x_node->y_size > BSC_Y * 4)
		{
			split_x_node(cube, w_index, x_index);

			if (w_node->x_size > BSC_X * 4)
			{
				split_w_node(cube, w_index);
			}
		}
	}

	if (x_node->volume + BSC_Z < x_node->y_size * BSC_Z)
	{
		deflate_x_axis(cube, w_index, x_index);

		if (w_node->volume + BSC_Z * BSC_Y < w_node->x_size * BSC_Z * BSC_Y)
		{
			deflate_w_axis(cube, w_index);

			if (cube->volume + BSC_Z * BSC_Y * BSC_X < cube->w_size * BSC_Z * BSC_Y * BSC_X)
			{
				deflate_cube(cube);
			}
		}
	}
}

void remove_z_node(struct cube *cube, short w_index, short x_index, short y_index, short z_index)
{
	struct w_node *w_node = cube->w_axis[w_index];
	struct x_node *x_node = w_node->x_axis[x_index];
	struct y_node *y_node = x_node->y_axis[y_index];

	cube->volume--;

	w_node->volume--;
	x_node->volume--;

	y_node->z_size--;

	if (y_node->z_size != z_index)
	{
		memmove(&y_node->z_axis[z_index], &y_node->z_axis[z_index + 1], (y_node->z_size - z_index) * sizeof(struct z_node));
	}
	y_node->z_axis = (struct z_node *) realloc(y_node->z_axis, y_node->z_size * sizeof(struct z_node));

	if (y_node->z_size)
	{
		if (z_index == 0)
		{
			x_node->y_floor[y_index] = y_node->z_axis[z_index].key;

			if (y_index == 0)
			{
				w_node->x_floor[x_index] = y_node->z_axis[z_index].key;

				if (x_index == 0)
				{
					cube->w_floor[w_index] = y_node->z_axis[z_index].key;
				}
			}
		}

		if (x_node->volume + BSC_Z < x_node->y_size * BSC_Z)
		{
			deflate_x_axis(cube, w_index, x_index);

			if (w_node->volume + BSC_Z * BSC_Y < w_node->x_size * BSC_Z * BSC_Y)
			{
				deflate_w_axis(cube, w_index);

				if (cube->volume + BSC_Z * BSC_Y * BSC_X < cube->w_size * BSC_Z * BSC_Y * BSC_X)
				{
					deflate_cube(cube);
				}
			}
		}
	}
	else
	{
		remove_y_node(cube, w_index, x_index, y_index);
	}
}

void split_w_node(struct cube *cube, short w_index)
{
	struct w_node *w_node1, *w_node2;
	short x;
	int volume;

	insert_w_node(cube, w_index + 1);

	w_node1 = cube->w_axis[w_index];
	w_node2 = cube->w_axis[w_index + 1];

	w_node2->x_size = w_node1->x_size / 2;
	w_node1->x_size = w_node1->x_size - w_node2->x_size;

	w_node2->x_floor = (int *) realloc(w_node2->x_floor, w_node2->x_size * sizeof(int));
	w_node2->x_axis = (struct x_node **) realloc(w_node2->x_axis, w_node2->x_size * sizeof(struct x_node *));

	memcpy(&w_node2->x_floor[0], &w_node1->x_floor[w_node1->x_size], w_node2->x_size * sizeof(int));
	memcpy(&w_node2->x_axis[0], &w_node1->x_axis[w_node1->x_size], w_node2->x_size * sizeof(struct x_node *));

	w_node1->x_floor = (int *) realloc(w_node1->x_floor, w_node1->x_size * sizeof(int));	
	w_node1->x_axis = (struct x_node **) realloc(w_node1->x_axis, w_node1->x_size * sizeof(struct x_node *));

	for (x = volume = 0 ; x < w_node1->x_size ; x++)
	{
		volume += w_node1->x_axis[x]->volume;
	}

	w_node2->volume = w_node1->volume - volume;
	w_node1->volume = volume;

	cube->w_floor[w_index + 1] = w_node2->x_floor[0];
}

void merge_w_node(struct cube *cube, short w_index1, short w_index2)
{
	struct w_node *w_node1 = cube->w_axis[w_index1];
	struct w_node *w_node2 = cube->w_axis[w_index2];

	w_node1->x_floor = (int *) realloc(w_node1->x_floor, (w_node1->x_size + w_node2->x_size) * sizeof(int));
	w_node1->x_axis = (struct x_node **) realloc(w_node1->x_axis, (w_node1->x_size + w_node2->x_size) * sizeof(struct x_node *));

	memcpy(&w_node1->x_floor[w_node1->x_size], &w_node2->x_floor[0], w_node2->x_size * sizeof(int));
	memcpy(&w_node1->x_axis[w_node1->x_size], &w_node2->x_axis[0], w_node2->x_size * sizeof(struct x_node *));

	w_node1->x_size = w_node1->x_size + w_node2->x_size;

	w_node1->volume += w_node2->volume;

	remove_w_node(cube, w_index2);
}

void split_x_node(struct cube *cube, short w_index, short x_index)
{
	struct x_node *x_node1, *x_node2;
	short y;
	int volume;

	insert_x_node(cube, w_index, x_index + 1);

	x_node1 = cube->w_axis[w_index]->x_axis[x_index];
	x_node2 = cube->w_axis[w_index]->x_axis[x_index + 1];

	x_node2->y_size = x_node1->y_size / 2;
	x_node1->y_size = x_node1->y_size - x_node2->y_size;

	x_node2->y_floor = (int *) realloc(x_node2->y_floor, x_node2->y_size * sizeof(int));
	x_node2->y_axis = (struct y_node **) realloc(x_node2->y_axis, x_node2->y_size * sizeof(struct y_node *));

	memcpy(&x_node2->y_floor[0], &x_node1->y_floor[x_node1->y_size], x_node2->y_size * sizeof(int));
	memcpy(&x_node2->y_axis[0], &x_node1->y_axis[x_node1->y_size], x_node2->y_size * sizeof(struct y_node *));

	x_node1->y_floor = (int *) realloc(x_node1->y_floor, x_node1->y_size * sizeof(int));
	x_node1->y_axis = (struct y_node **) realloc(x_node1->y_axis, x_node1->y_size * sizeof(struct y_node *));

	for (y = volume = 0 ; y < x_node1->y_size ; y++)
	{
		volume += x_node1->y_axis[y]->z_size;
	}

	x_node2->volume = x_node1->volume - volume;
	x_node1->volume = volume;

	cube->w_axis[w_index]->x_floor[x_index + 1] = x_node2->y_floor[0];
}

void merge_x_node(struct cube *cube, short w_index, short x_index1, short x_index2)
{
	struct x_node *x_node1 = cube->w_axis[w_index]->x_axis[x_index1];
	struct x_node *x_node2 = cube->w_axis[w_index]->x_axis[x_index2];

	x_node1->y_floor = (int *) realloc(x_node1->y_floor, (x_node1->y_size + x_node2->y_size) * sizeof(int));
	x_node1->y_axis = (struct y_node **) realloc(x_node1->y_axis, (x_node1->y_size + x_node2->y_size) * sizeof(struct y_node *));

	memcpy(&x_node1->y_floor[x_node1->y_size], &x_node2->y_floor[0], x_node2->y_size * sizeof(int));
	memcpy(&x_node1->y_axis[x_node1->y_size], &x_node2->y_axis[0], x_node2->y_size * sizeof(struct y_node *));

	x_node1->y_size = x_node1->y_size + x_node2->y_size;

	x_node1->volume += x_node2->volume;

	remove_x_node(cube, w_index, x_index2);
}


void split_y_node(struct cube *cube, short w_index, short x_index, short y_index)
{
	struct x_node *x_node = cube->w_axis[w_index]->x_axis[x_index];
	struct y_node *y_node1, *y_node2;

	insert_y_node(cube, w_index, x_index, y_index + 1);

	y_node1 = x_node->y_axis[y_index];
	y_node2 = x_node->y_axis[y_index + 1];

	y_node2->z_size = y_node1->z_size / 2;
	y_node1->z_size = y_node1->z_size - y_node2->z_size;

	y_node2->z_axis = (struct z_node *) realloc(y_node2->z_axis, y_node2->z_size * sizeof(struct z_node));

	memcpy(&y_node2->z_axis[0], &y_node1->z_axis[y_node1->z_size], y_node2->z_size * sizeof(struct z_node));

	y_node1->z_axis = (struct z_node *) realloc(y_node1->z_axis, y_node1->z_size * sizeof(struct z_node));

	x_node->y_floor[y_index + 1] = y_node2->z_axis[0].key;
}

void merge_y_node(struct cube *cube, short w_index, short x_index, short y_index1, short y_index2)
{
	struct y_node *y_node1 = cube->w_axis[w_index]->x_axis[x_index]->y_axis[y_index1];
	struct y_node *y_node2 = cube->w_axis[w_index]->x_axis[x_index]->y_axis[y_index2];

	y_node1->z_axis = (struct z_node *) realloc(y_node1->z_axis, (y_node1->z_size + y_node2->z_size) * sizeof(struct z_node));

	memcpy(&y_node1->z_axis[y_node1->z_size], &y_node2->z_axis[0], y_node2->z_size * sizeof(struct z_node));

	y_node1->z_size = y_node1->z_size + y_node2->z_size;

	remove_y_node(cube, w_index, x_index, y_index2);
}


void deflate_x_axis(struct cube *cube, short w_index, short x_index)
{
	struct x_node *x_node = cube->w_axis[w_index]->x_axis[x_index];
	short y_index;

	for (y_index = 0 ; y_index + 1 < x_node->y_size ; y_index++)
	{
		if (x_node->y_axis[y_index]->z_size + x_node->y_axis[y_index + 1]->z_size < BSC_Z * 2)
		{
			merge_y_node(cube, w_index, x_index, y_index, y_index + 1);
		}
	}
}


void deflate_w_axis(struct cube *cube, short w_index)
{
	struct w_node *w_node = cube->w_axis[w_index];
	short x_index, d_index;

	for (x_index = d_index = 0 ; x_index + 1 < w_node->x_size ; x_index++)
	{
		if (w_node->x_axis[d_index]->y_size + w_node->x_axis[d_index + 1]->y_size > w_node->x_axis[x_index]->y_size + w_node->x_axis[x_index + 1]->y_size)
		{
			d_index = x_index;
		}
	}

	merge_x_node(cube, w_index, d_index, d_index + 1);
}

void deflate_cube(struct cube *cube)
{
	short w_index, d_index;

	for (w_index = d_index = 0 ; w_index + 1 < cube->w_size ; w_index++)
	{
		if (cube->w_axis[d_index]->x_size + cube->w_axis[d_index + 1]->x_size > cube->w_axis[w_index]->x_size + cube->w_axis[w_index + 1]->x_size)
		{
			d_index = w_index;
		}
	}

	merge_w_node(cube, d_index, d_index + 1);
}

#if 0
void show_cube(struct cube *cube, short depth)
{
	struct w_node *w_node;
	struct x_node *x_node;
	struct y_node *y_node;
	struct z_node *z_node;
	short w, x, y, z;

	for (w = 0 ; w < cube->w_size ; w++)
	{
		w_node = cube->w_axis[w];

		if (depth == 1)
		{
			printf("w index [%3d] x size [%3d] volume [%10d]\n", w, w_node->x_size, w_node->volume);

			continue;
		}

		for (x = 0 ; x < w_node->x_size ; x++)
		{
			x_node = w_node->x_axis[x];

			if (depth == 2)
			{
				printf("w [%3d] x [%3d] s [%3d] v [%10d]\n", w, x, x_node->y_size, x_node->volume);

				continue;
			}

			for (y = 0 ; y < x_node->y_size ; y++)
			{
				y_node = x_node->y_axis[y];

				if (depth == 3)
				{
					printf("w [%3d] x [%3d] y [%3d] s [%3d]\n", w, x, y, y_node->z_size);

					continue;
				}

				for (z = 0 ; z < y_node->z_size ; z++)
				{
					z_node = &y_node->z_axis[z];

					printf("w [%3d] x [%3d] y [%3d] z [%3d] [%010d] (%s)\n", w, x, y, z, z_node->key, z_node->val);
				}
			}
		}
	}
}

long long utime()
{
	struct timeval now_time;

	gettimeofday(&now_time, NULL);

	return now_time.tv_sec * 1000000LL + now_time.tv_usec;
}

int main(int argc, char **argv)
{
	int cnt, max;
	long long start, end;

	struct cube *cube;

	if (argv[1] && *argv[1])
	{
		printf("%s\n", argv[1]);
	}

	cube = create_cube();

	max = 10000000;

	start = utime();

	srand(10);

	for (cnt = 1 ; cnt <= max ; cnt++)
	{
//		set_key(cube, 0 + rand(), "");
//		set_key(cube, 0 + cnt, "fwd order");
		set_key(cube, max - cnt, "rev order");
	}

	end = utime();

	printf("Time to insert %d elements: %f seconds.\n", max, (end - start) / 1000000.0);

	show_cube(cube, 1);

	start = utime();

	return 0;

	srand(10);

	for (cnt = 1 ; cnt <= max ; cnt++)
	{
//		set_key(cube, 0 + cnt, "fwd order");
//		set_key(cube, 0 + rand(), "");
//		set_key(cube, 0 - cnt, "upd order");
//		del_index(cube, rand() % cube->volume);
//		del_index(cube, cnt);
//		del_index(cube, 0);
	}

	end = utime();

	printf("Time to update %d elements: %f seconds.\n", max, (end - start) / 1000000.0);

	show_cube(cube, 1);

//	destroy_cube(cube);

	return 0;
}
#endif

