#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <unistd.h>
#include <inttypes.h>

#include <termios.h>

#define DELAY 100000

typedef struct option
{
	char arg1[3];
	char arg2[32];
	char info[256];
} option;

// Структура поля игры "Жизнь"
typedef struct field
{
	int8_t **point;
	size_t rows, cols;
} field;

// Структура правил игры "Жизнь"
typedef struct rule
{
	int16_t b, s;
} rule;

// Коды завершения программы
enum error {
	NORMAL,
	UNKNOWN_ARG,
	MISSING_PARAM,
	INCORRECT_RULE,
	INCORRECT_FILE
};

// Аргументы коммандной строки
const option options[] = {
	{"-r", "--rule", "Rule using for game of life. Format: B<number>/S<number>"},
	{"-f", "--file", "Using initial pattern from file. Format: <path/to/file>"},
	{"-h", "--help", "Print current help page."},
	{"", "", ""}
};

// Заголовок страницы справки
const char help_promt[] = "This is simple realisation of Conway's Game of Life.";

// Глобальные переменные: структура параметров окна терминала, структура поля, структура правил
struct winsize ws;
struct field fl = {.point = NULL, .rows = 0, .cols = 0};
struct field init = {.point = NULL, .rows = 0, .cols = 0};
struct rule r = {.b = 0b000001000, .s = 0b000001100};
int8_t enable_step = 1;

void echo_disable(int8_t flag)
{
	struct termios setup;
	tcgetattr(fileno(stdin), &setup);
	if (flag) setup.c_lflag &= ~ECHO;
	else setup.c_lflag |= ECHO;
	tcsetattr(fileno(stdin), 0, &setup);
}

// Выход из программы
void final (enum error code, char *msg)
{
	switch (code)
	{
		case UNKNOWN_ARG:
			printf("Unknown argument %s\n", msg);
			break;
		case MISSING_PARAM:
			printf("Missing parameter to %s argument\n", msg);
			break;
		case INCORRECT_RULE:
			printf("Incorrect rule: %s\n", msg);
			break;
		case INCORRECT_FILE:
			printf("Incorrect file name: %s\n", msg);
			break;
		default:
			break;
	}

	echo_disable(0);
	exit(code);
}


// Получение размеров окна терминала (с учётом зарезервированной строки для информации об игре)
void get_winsize (struct winsize *ws)
{
	ioctl(0, TIOCGWINSZ, ws);
	ws->ws_row -= 1;
}

// Вывод поля игры
void field_out ()
{
	printf("\e[0;0H");

	for (size_t i = 0; i < fl.rows; i += 4)
	{
		for (size_t j = 0; j < fl.cols; j += 2)
		{
			// Формирование символа шрифта Брайля
			char pattern[4] = {0xE2, 0xA0, 0x80, 0};
			pattern[2] +=
				(fl.point[i][j] << 0) +
				(fl.point[i+1][j] << 1) +
				(fl.point[i+2][j] << 2) +
				(fl.point[i][j+1] << 3) +
				(fl.point[i+1][j+1] << 4) +
				(fl.point[i+2][j+1] << 5);
			pattern[1] +=
				(fl.point[i+3][j] << 0) +
				(fl.point[i+3][j+1] << 1);

			printf("%s", pattern);
		}
		printf("\n");
	}

	// Формирование номеров правил
	int B = 0, S = 0;
	for(size_t i = 0; i <= 8; i++)
	{
		int16_t mask = 1 << i;
		if(r.b & mask) B = B * 10 + i;
		if(r.s & mask) S = S * 10 + i;
	}

	printf("Field: %ldx%ld Rule: B%d/S%d\t        \e[0;0H", fl.cols, fl.rows, B, S);
}

// очистка поля по SIGINT
void field_clean (int signum)
{
	printf("\e[2J\e[0;0H");
	printf("~Made by Allonny~\n");
	final(NORMAL, NULL);
}

// Случайное заполенение поля
void field_random_fill ()
{
	for (size_t i = 0; i < fl.rows; i++)
		for (size_t j = 0; j < fl.cols; j++)
			fl.point[i][j] = (rand() & 0b11) >= 0b11;
}

// Заполнение поля заранее заданным узором
void field_pattern_fill ()
{
	for (size_t i = 0; i < fl.rows; i++)
		for (size_t j = 0; j < fl.cols; j++)
			fl.point[i][j] = (i < init.rows && j < init.cols) ? init.point[i][j] : 0;
}

// Заполнение поля
void field_fill ()
{
	if (init.point) field_pattern_fill();
	else field_random_fill();
}

// Инициализация поля
void field_init ()
{
	for (size_t i = 0; i < fl.rows; i++)
		free(fl.point[i]);
	free(fl.point);

	get_winsize(&ws);
	fl.rows = ws.ws_row << 2;
	fl.cols = ws.ws_col << 1;
	fl.point = calloc(fl.rows, sizeof(*fl.point));
	for (size_t i = 0; i < fl.rows; i++)
		fl.point[i] = calloc(fl.cols, sizeof(**fl.point));
}

// Шаг симуляции игры "Жизнь"
void field_step ()
{
	int8_t new_field[fl.rows][fl.cols];
	int16_t alive;
	ssize_t u, v, p, q;

	for (ssize_t i = 0; i < fl.rows; i++)
	{
		// Учитываются особые случаи на краях поля, топология которого эквивалентна тору
		// Случаи верхней и нижней строк
		u = i-1 < 0 ? fl.rows-1 : i-1;
		v = i+1 < fl.rows ? i+1 : 0;

		for (ssize_t j = 0; j < fl.cols; j++)
		{
			// Случаи крайних левого и правого столбцов
			p = j-1 < 0 ? fl.cols-1 : j-1;
			q = j+1 < fl.cols ? j+1 : 0;

			// Подсчёт живых соседей и применение правил игры
			alive = 1 << (fl.point[u][j] + fl.point[v][j] + fl.point[i][p] + fl.point[i][q] +
				fl.point[u][p] + fl.point[v][q] + fl.point[u][q] + fl.point[v][p]);
			new_field[i][j] = (fl.point[i][j] ? r.s & alive : r.b & alive) > 0;
		}
	}

	// Применение новых значений на поле
	for (size_t i = 0; i < fl.rows; i++)
		for (size_t j = 0; j < fl.cols; j++)
			fl.point[i][j] = new_field[i][j];
}

// Изменение размеров окна
void resize (int signum)
{
	field_init();
	field_fill();
}

void enable_toggle (int signum)
{
	enable_step = enable_step ? 0 : 1;
}

// Парсинг текстового представления правила в формате: B<натуральное число>/S<натуральное числыыо>
int rule_parse (char *arg)
{
	size_t B, S;
	if (sscanf(arg, "B%ld/S%ld", &B, &S) != 2)
		return 1;

	for (r.b = 0; B; B /= 10)
		r.b |= 1 << (B % 10);
	
	for (r.s = 0; S; S /= 10)
		r.s |= 1 << (S % 10);

	return 0;
}

// Считывание текстового файла для задания стартового узора
int file_parse (char *arg)
{
	for (size_t i = 0; i < init.rows; i++)
		free(init.point[i]);
	free(init.point);

	FILE *f;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	f = fopen(arg, "r");
	if (!f) return 1;
	for (init.rows = 0, init.cols = 0; (read = getline(&line, &len, f)) != -1; init.rows++)
		init.cols = init.cols > read - 1 ? init.cols : read - 1;

	init.point = calloc(init.rows, sizeof(*init.point));
	for (size_t i = 0; i < init.rows; i++)
		init.point[i] = calloc(init.cols, sizeof(**init.point));

	fseek(f, 0, SEEK_SET);
	for (size_t i = 0; (read = getline(&line, &len, f)) != -1; i++)
		for (size_t j = 0; j < init.cols; j++)
			init.point[i][j] = j < read ? !(line[j] == ' ' || line[j] == '\n') : 0;

	fclose(f);
	if(line) free(line);

	return 0;
}

// Вывод справки
int help_out ()
{
	printf("%s\n", help_promt);
	for (size_t i = 0; strlen(options[i].arg1); i++)
	{
		printf("\n  %s  %s\n", options[i].arg1, options[i].arg2);
		printf("\t%s\n", options[i].info);
	}

	return 0;
}

// Парсинг аргументов коммандной строки
void args_parse (int argc, char *argv[])
{
	for	(size_t i = 1; i < argc; i++)
	{
		if (!(strcmp(argv[i], options[0].arg1) && strcmp(argv[i], options[0].arg2)))
		{
			if (++i >= argc) final(MISSING_PARAM, argv[i-1]);
			if (rule_parse(argv[i])) final(INCORRECT_RULE, argv[i]);
			continue;
		}
		if (!(strcmp(argv[i], options[1].arg1) && strcmp(argv[i], options[1].arg2)))
		{
			if (++i >= argc) final(MISSING_PARAM, argv[i-1]);
			if (file_parse(argv[i])) final(INCORRECT_FILE, argv[i]);
			continue;
		}
		if (!(strcmp(argv[i], options[2].arg1) && strcmp(argv[i], options[2].arg2)))
		{
			help_out();
			final(NORMAL, NULL);
		}
		final(UNKNOWN_ARG, argv[i]);
	}
}

// Главный вход
int main (int argc, char *argv[])
{
	// Получение правила из аргумента коммандной строки
	if (argc > 1)
		args_parse(argc, argv);

	srand(time(NULL));

	// Объявление слушаетлей сигналов
	signal(SIGINT, field_clean);
	signal(SIGWINCH, resize);
	signal(SIGTSTP, enable_toggle);

	echo_disable(1);

	// Начальные инициализация и заполнение поля
	field_init();
	field_fill();
	field_out();

	// Цикл игры
	while (1)
	{
		usleep(DELAY);
		if(enable_step) field_step();
		field_out();
	}

	field_clean(0);
	return 0;
}
