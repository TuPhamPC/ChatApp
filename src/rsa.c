#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

char buffer[1024];
const int MAX_DIGITS = 50;
int i, j = 0;

struct public_key_class
{
  long long modulus;
  long long exponent;
};

struct private_key_class
{
  long long modulus;
  long long exponent;
};

// Tìm ước chung lớn nhất
long long gcd(long long a, long long b)
{
  long long c;
  while (a != 0)
  {
    c = a;
    a = b % a;
    b = c;
  }
  return b;
}

//thuật toán Euclid mở rộng (Extended Euclidean Algorithm).
long long ExtEuclid(long long a, long long b)
{
  long long x = 0, y = 1, u = 1, v = 0, gcd = b, m, n, q, r;
  while (a != 0)
  {
    q = gcd / a;
    r = gcd % a;
    m = x - u * q;
    n = y - v * q;
    gcd = a;
    a = r;
    x = u;
    y = v;
    u = m;
    v = n;
  }
  return y;
}
static inline long long modmult(long long a, long long b, long long mod)
{
  // Kiểm tra nếu a =0 trả về 0
  if (a == 0)
  {
    return 0;
  }
  register long long product = a * b;
  // if multiplication does not overflow, we can use it
  if (product / a == b)
  {
    return product % mod;
  }
  // if a % 2 == 1 i. e. a >> 1 is not a / 2
  if (a & 1)
  {
    product = modmult((a >> 1), b, mod);
    if ((product << 1) > product)
    {
      return (((product << 1) % mod) + b) % mod;
    }
  }
  // implicit else
  product = modmult((a >> 1), b, mod);
  if ((product << 1) > product)
  {
    return (product << 1) % mod;
  }
  // implicit else: this is about 10x slower than the code above, but it will not overflow
  long long sum;
  sum = 0;
  while (b > 0)
  {
    if (b & 1)
      sum = (sum + a) % mod;
    a = (2 * a) % mod;
    b >>= 1;
  }
  return sum;
}

//thực hiện phép tính lũy thừa modulo trong quá trình triển khai thuật toán RSA b^e mod m
long long rsa_modExp(long long b, long long e, long long m)
{
  long long product;
  product = 1;
  if (b < 0 || e < 0 || m <= 0)
  {
    return -1;
  }
  b = b % m;
  while (e > 0)
  {
    if (e & 1)
    {
      product = modmult(product, b, m);
    }
    b = modmult(b, b, m);
    e >>= 1;
    // printf("%lld ", e);
  } // printf("\n");
  return product;
}

// tạo ra một khóa công khai và một khóa riêng tư và lưu chúng vào các con trỏ được cung cấp.
void rsa_gen_keys(struct public_key_class *pub, struct private_key_class *priv, char *PRIME_SOURCE_FILE)
{
  FILE *primes_list;
  if (!(primes_list = fopen(PRIME_SOURCE_FILE, "r")))
  {
    fprintf(stderr, "Problem reading %s\n", PRIME_SOURCE_FILE);
    exit(1);
  }

  // Đếm số lượng số nguyên tố trong danh sách.
  long long prime_count = 0;
  do
  {
    int bytes_read = fread(buffer, 1, sizeof(buffer) - 1, primes_list);
    buffer[bytes_read] = '\0';
    for (i = 0; buffer[i]; i++)
    {
      if (buffer[i] == '\n')
      {
        prime_count++;
      }
    }
  } while (feof(primes_list) == 0);

  // Chọn ngẫu nhiên số nguyên tố từ danh sách, lưu chúng vào p và q.
  long long p = 0;
  long long q = 0;

  // values of e should be sufficiently large to protect against naive attacks
  long long e = (2 << 16) + 1;
  long long d = 0;
  char prime_buffer[MAX_DIGITS];
  long long max = 0;
  long long phi_max = 0;

  srand(time(NULL));

  do
  {
    // a và b là các vị trí của p và q trong danh sách.
    int a = (double)rand() * (prime_count + 1) / (RAND_MAX + 1.0);
    int b = (double)rand() * (prime_count + 1) / (RAND_MAX + 1.0);

    // Ở đây, chúng ta tìm số nguyên tố tại vị trí a, và lưu nó dưới dạng p.
    rewind(primes_list);
    for (i = 0; i < a + 1; i++)
    {
      //  for(j=0; j < MAX_DIGITS; j++){
      //	prime_buffer[j] = 0;
      //  }
      fgets(prime_buffer, sizeof(prime_buffer) - 1, primes_list);
    }
    p = atol(prime_buffer);

    // Ở đây, chúng ta tìm số nguyên tố tại vị trí b, và lưu nó dưới dạng q.
    rewind(primes_list);
    for (i = 0; i < b + 1; i++)
    {
      for (j = 0; j < MAX_DIGITS; j++)
      {
        prime_buffer[j] = 0;
      }
      fgets(prime_buffer, sizeof(prime_buffer) - 1, primes_list);
    }
    q = atol(prime_buffer);

    max = p * q;
    phi_max = (p - 1) * (q - 1);
  } while (!(p && q) || (p == q) || (gcd(phi_max, e) != 1));

  // Tiếp theo, chúng ta cần chọn a, b sao cho a*max+b*e = gcd(max,e). Thực tế, ở đây chúng ta chỉ cần b,
  // và giữ theo quy ước thường dùng trong RSA, chúng ta sẽ gọi nó là d. Chúng ta cũng muốn đảm bảo có
  // một biểu diễn của d là dương, do đó sử dụng vòng lặp while.
  d = ExtEuclid(phi_max, e);
  while (d < 0)
  {
    d = d + phi_max;
  }

  // printf("primes are %lld and %lld\n",(long long)p, (long long )q);
  // Bây giờ chúng ta lưu khóa công khai/riêng tư vào structs tương ứng
  pub->modulus = max;
  pub->exponent = e;

  priv->modulus = max;
  priv->exponent = d;
}

//mã hóa một thông điệp (message) sử dụng khóa công khai trong hệ thống mã hóa RSA
long long *rsa_encrypt(const char *message, const unsigned long message_size,
                       const struct public_key_class *pub)
{
  // printf("%lld %lld\n", pub->exponent, pub->modulus);
  long long *encrypted = malloc(sizeof(long long) * message_size);
  memset(encrypted, '\0', sizeof(*encrypted));
  if (encrypted == NULL)
  {
    fprintf(stderr,
            "Error: Heap allocation failed.\n");
    return NULL;
  }
  long long i = 0;
  for (i = 0; i < message_size; i++)
  {
    encrypted[i] = rsa_modExp(message[i], pub->exponent, pub->modulus);
    printf("%d %lld\n", message[i], encrypted[i]);
    if (encrypted[i] == -1)
    {
      printf("%d %lld %lld %lld\n", message[i], pub->exponent, pub->modulus, encrypted[i]);
      printf("Some shit happenned!\n");
      return NULL;
    }
  }
  return encrypted;
}

//giải mã một dãy số đã được mã hóa (message) sử dụng khóa riêng tư trong hệ thống mã hóa RSA
char *rsa_decrypt(const long long *message,
                  const unsigned long message_size,
                  const struct private_key_class *priv)
{
  if (message_size % sizeof(long long) != 0)
  {
    fprintf(stderr,
            "Error: message_size is not divisible by %d, so cannot be output of rsa_encrypt\n", (int)sizeof(long long));
    return NULL;
  }

  // cấp phát bộ nhớ để thực hiện quá trình giải mã (temp)
  // và không gian lưu trữ kết quả dưới dạng mảng ký tự (decrypted)

  char *decrypted = malloc(message_size / sizeof(long long));
  memset(decrypted, '\0', sizeof(*decrypted));
  char *temp = malloc(message_size);
  memset(temp, '\0', sizeof(*temp));
  if ((decrypted == NULL) || (temp == NULL))
  {
    fprintf(stderr,
            "Error: Heap allocation failed.\n");
    return NULL;
  }
  // đi qua từng khối 8-byte và thực hiện quá trình giải mã.
  long long i = 0;
  for (i = 0; i < message_size / 8; i++)
  {
    if(message[i] == '\0') break;
    if ((temp[i] = rsa_modExp(message[i], priv->exponent, priv->modulus)) == -1)
    {
      free(temp);
      return NULL;
    }
  }
  // Kết quả nên là một số thuộc phạm vi của ký tự, từ đó ta có thể lấy lại byte ban đầu.
 // Chúng ta đặt giá trị đó vào mảng decrypted, sau đó trả về.

  for (i = 0; i < message_size / 8; i++)
  {
    if(temp[i] <= 0) break;
    decrypted[i] = temp[i];
    printf("%c %d\n", decrypted[i], decrypted[i]);
  } //printf("\n");
  decrypted[i] = '\0';
  free(temp);
  return decrypted;
}
