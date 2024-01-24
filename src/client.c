#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

char my_username[USERNAME_SIZE];
char curr_group_name[GROUP_NAME_SIZE];
int curr_group_id = -1;
int join_succ = 0;

char *PRIME_SOURCE_FILE = "../assets/primes.txt";

struct public_key_class my_pub[1];
struct private_key_class my_priv[1];

Public_key_users user_pub[1];

int doing = 0;

// tạo và thiết lập một kết nối TCP đến
int connect_to_server()
{
    
    int client_socket;
    struct sockaddr_in server_addr;

    // Thiết lập thông tin địa chỉ của server
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    // Tạo socket TCP
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        // Báo lỗi nếu tạo socket thất bại
        report_err(ERR_SOCKET_INIT);
        exit(0);
    }

    // Kết nối đến server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        report_err(ERR_CONNECT_TO_SERVER);
        exit(0);
    }
     // Trả về số hiệu socket cho kết nối đã thiết lập
    return client_socket;
}

void login_menu()
{
    printf("------ Welcome to chat app ------\n");
    printf("1. Login\n");
    // printf("2. Sign up\n");
    printf("2. Exit\n");
    // printf("Your choice: ");
}

void user_menu()
{
    printf("\n\n****** Login success ******\n");
    // printf("1. Show current online users\n");
    printf("1. Private chat\n");
    printf("2. Chat All\n");
    printf("3. Logout\n");
    printf("4. Show online users\n");
    printf("5. Group chat\n");
    // printf("Your choice: ");
}

//  nhom chat menu 
void group_chat_menu()
{
    printf("\n\n****** Group chat ******\n");
    printf("1. Show my group\n");
    printf("2. Make new group\n");
    printf("3. Join group\n");
    printf("4. Return main menu\n");
}

void sub_group_chat_menu(char *group_name)
{
    printf("\n\n****** %s ******\n", group_name);
    printf("1. Invite your friends\n");
    printf("2. Chat \n");
    printf("3. Show group infomation \n");
    printf("4. Leave the group chat\n");
    printf("5. View chat history\n");
    printf("6. Return group chat menu\n");
}
// void ask_server(int client_socket)
// {

//     int choice, result;
//     Package pkg;

//     while (1)
//     {
//         sleep(1);
//         login_menu();
//         printf("Your choice: ");
//         scanf("%d", &choice);
//         clear_stdin_buff();

//         switch (choice)
//         {
//         case 1:
//             pkg.ctrl_signal = LOGIN_REQ;
//             send(client_socket, &pkg, sizeof(pkg), 0);
//             result = login(client_socket);
//             if (result == LOGIN_SUCC)
//             {
//                 user_use(client_socket);
//             }
//             else if (result == INCORRECT_ACC)
//             {
//                 report_err(ERR_INCORRECT_ACC);
//             }
//             else
//             {
//                 report_err(ERR_SIGNED_IN_ACC);
//             }
//             break;
//         case 2:
//             pkg.ctrl_signal = QUIT_REQ;
//             send(client_socket, &pkg, sizeof(pkg), 0);
//             close(client_socket);
//             exit(0);
//         }
//     }
// }

int login(int client_socket, char *username, char *password)
{
    // Tạo một đối tượng Package để chứa thông điệp gửi và nhận
    Package pkg;

    // Gửi tên người dùng đến máy chủ
    strcpy(pkg.msg, username);
    send(client_socket, &pkg, sizeof(pkg), 0);

    // Nhận phản hồi từ máy chủ sau khi gửi tên người dùng
    recv(client_socket, &pkg, sizeof(pkg), 0);

    // Gửi mật khẩu đến máy chủ
    strcpy(pkg.msg, password);
    send(client_socket, &pkg, sizeof(pkg), 0);

    // Nhận phản hồi từ máy chủ sau khi gửi mật khẩu
    recv(client_socket, &pkg, sizeof(pkg), 0);

    // Nếu quá trình đăng nhập thành công
    if (pkg.ctrl_signal == LOGIN_SUCC){
        // Sao chép tên người dùng đã đăng nhập vào biến global
        strcpy(my_username, username);

        // Nếu khóa cá nhân chưa được tạo, hãy tạo nó và gửi khóa công khai đến máy chủ
        if(my_priv->exponent == 0)
            rsa_gen_keys(my_pub, my_priv, PRIME_SOURCE_FILE);
        
        // Hiển thị thông tin khóa cá nhân và công khai
        printf("Khóa Cá Nhân:\n Modulus: %lld\n Exponent: %lld\n", (long long)my_priv->modulus, (long long)my_priv->exponent);
        printf("Khóa Công Khai:\n Modulus: %lld\n Exponent: %lld\n\n", (long long)my_pub->modulus, (long long)my_pub->exponent);

        // Gửi khóa công khai của người dùng đến máy chủ
        send_my_public_key(client_socket);
    }

    // Trả về tín hiệu điều khiển để xác định kết quả của quá trình đăng nhập
    return pkg.ctrl_signal;
}


// Hàm gửi khóa công khai của người dùng đến máy chủ
void send_my_public_key(int client_socket) {
    // Tạo một đối tượng Package để chứa thông điệp gửi
    Package pkg;

    // Đặt tín hiệu điều khiển để thông báo là đang gửi khóa công khai
    pkg.ctrl_signal = SEND_PUBLIC_KEY;

    // Sao chép tên người dùng đã đăng nhập vào gói tin
    strcpy(pkg.sender, my_username);

    // Sao chép modulus và exponent của khóa công khai vào gói tin
    memcpy(pkg.msg, &my_pub->modulus, sizeof(my_pub->modulus));
    memcpy(pkg.msg + sizeof my_pub->modulus, &my_pub->exponent, sizeof(my_pub->exponent));

    // Gửi gói tin chứa khóa công khai đến máy chủ
    send(client_socket, &pkg, sizeof(pkg), 0);
}


// Hàm nhận khóa công khai từ một người dùng khác thông qua gói tin Package
void receive_public_key(int client_socket, Package* pkg) {
    // Sao chép tên người dùng đã gửi khóa công khai vào biến user_pub
    strcpy(user_pub->username, pkg->receiver);

    // Sao chép exponent và modulus của khóa công khai từ gói tin vào biến user_pub
    user_pub->public_key->exponent = ((struct public_key_class*)pkg->msg)->exponent;
    user_pub->public_key->modulus = ((struct public_key_class*)pkg->msg)->modulus;

    // Hiển thị thông tin khóa công khai nhận được từ người dùng
    printf("Nhận khóa công khai của %s: %lld %lld\n", user_pub->username, (long long)user_pub->public_key->exponent, (long long)user_pub->public_key->modulus);
}


// Hàm kiểm tra khóa công khai của một người dùng
int check_public_key(int client_socket, char* username) {
    // So sánh tên người dùng đã lưu với tên người dùng cần kiểm tra
    if(strcmp(user_pub->username, username) == 0) {
        // Trả về 1 nếu khóa công khai đã được lưu trữ
        return 1;
    } else {
        // Nếu khóa công khai chưa được lưu trữ, gửi yêu cầu khóa công khai đến người dùng khác
        Package pkg;
        strcpy(pkg.sender, my_username);
        strcpy(pkg.receiver, username);
        pkg.ctrl_signal = SEND_PUBLIC_KEY_REQ;
        send(client_socket, &pkg, sizeof(pkg), 0);
        
        // Trả về 0 để thông báo rằng đang chờ nhận khóa công khai từ người dùng khác
        return 0;
    }
}


// void user_use(int client_socket)
// {
    // printf("Login successfully!\n");
    // int login = 1;
    // int choice, result;
    // Package pkg;

    // pthread_t read_st;
    // if (pthread_create(&read_st, NULL, read_msg, (void *)&client_socket) < 0)
    // {
    //     report_err(ERR_CREATE_THREAD);
    //     exit(0);
    // }
    // pthread_detach(read_st);

    // see_active_user(client_socket);

    // while (login)
    // {

    //     user_menu();
    //     printf("Your choice: \n");
    //     scanf("%d", &choice);
    //     clear_stdin_buff();

        // switch (choice)
        // {
        // case 1:
        //     private_chat(client_socket);
        //     break;

        // case 2:
        //     chat_all(client_socket);
        //     break;

        // case 3:
        //     login = 0;
        //     pkg.ctrl_signal = LOG_OUT;
        //     // strcpy(pkg.sender, my_username);
        //     send(client_socket, &pkg, sizeof(pkg), 0);
        //     strcpy(my_username, "x");
        //     strcpy(curr_group_name, "x");
        //     curr_group_id = -1;
        //     sleep(1);
        //     break;
        // case 4:
        //     see_active_user(client_socket);
        //     break;
        // 17/01/2023
        // case 5:
        //     group_chat_init(client_socket);
        //     break;
        // default:
        //     printf("Ban nhap sai roi !\n");
        //     break;
        // }
    // }
// }

// void *read_msg(void *param)
// {
//     int *c_socket = (int *)param;
//     int client_socket = *c_socket;
//     // printf("\nmysoc: %d\n", client_socket);
//     // int client_socket = my_socket;
//     Package pkg;
//     while (1)
//     {
//         recv(client_socket, &pkg, sizeof(pkg), 0);
//         // printf("receive %d from server\n", pkg.ctrl_signal);
//         switch (pkg.ctrl_signal)
//         {
//         case SHOW_USER:
//             printf("Current online users: %s \n", pkg.msg);
//             break;

//         case PRIVATE_CHAT:
//             printf("%s: %s\n", pkg.sender, pkg.msg);
//             break;

//         case CHAT_ALL:
//             printf("%s to all: %s\n", pkg.sender, pkg.msg);
//             break;

//         case ERR_INVALID_RECEIVER:
//             report_err(ERR_INVALID_RECEIVER);
//             break;
//         case MSG_SENT_SUCC:
//             printf("Message sent!\n");
//             break;
//         case GROUP_CHAT_INIT:
//             printf("%s\n", pkg.msg);
//             break;
//         case SHOW_GROUP:
//             printf("Your group: \n%s \n", pkg.msg);
//             break;

//         case MSG_MAKE_GROUP_SUCC:
//             printf("Your new group: %s \n", pkg.msg);
//             break;
//         case JOIN_GROUP_SUCC:
//             printf("Current group: %s \n", pkg.msg);
//             strcpy(curr_group_name, pkg.msg);
//             curr_group_id = pkg.group_id;
//             join_succ = 1;
//             break;
//         case INVITE_FRIEND:
//             printf("Attention: %s \n", pkg.msg);
//             break;
//         case ERR_GROUP_NOT_FOUND:
//             report_err(ERR_GROUP_NOT_FOUND);
//             break;
//         case ERR_IVITE_MYSELF:
//             report_err(ERR_IVITE_MYSELF);
//             break;
//         case ERR_USER_NOT_FOUND:
//             report_err(ERR_USER_NOT_FOUND);
//             break;
//         case ERR_FULL_MEM:
//             report_err(ERR_FULL_MEM);
//             break;
//         case INVITE_FRIEND_SUCC:
//             printf("%s\n", pkg.msg);
//             break;
//         case GROUP_CHAT:
//             if (curr_group_id == pkg.group_id)
//             {
//                 printf("%s: %s\n", pkg.sender, pkg.msg);
//             }
//             else
//             {
//                 printf("%s sent to Group_%d: %s\n", pkg.sender, pkg.group_id, pkg.msg);
//             }
//             break;
//         case SHOW_GROUP_NAME:
//             printf("GROUP NAME: %s\n", pkg.msg);
//             break;
//         case SHOW_GROUP_MEM:
//             printf("%s\n", pkg.msg);
//             break;
//         case LEAVE_GROUP_SUCC:
//             printf("%s\n", pkg.msg);
//             break;
//         case LOG_OUT:
//             sleep(1);
//             pthread_exit(NULL);
//             break;
//         default:
//             break;
//         }
//     }
// }

// Hàm yêu cầu máy chủ hiển thị danh sách người dùng đang hoạt động
void see_active_user(int client_socket)
{
    // Tạo một đối tượng Package để chứa yêu cầu hiển thị danh sách người dùng
    Package pkg;
    pkg.ctrl_signal = SHOW_USER;

    // Gửi yêu cầu đến máy chủ
    send(client_socket, &pkg, sizeof(pkg), 0);

}


void make_done(int msg) {
    doing = msg;
}

// Hàm kiểm tra trạng thái của người nhận tin nhắn riêng
int check_receiver(int client_socket, char* receiver) {
    int res;

    // Tạo một đối tượng Package để chứa thông điệp gửi
    Package pkg;

    // Đặt thông điệp và tên người gửi và nhận vào gói tin
    strcpy(pkg.receiver, receiver);
    strcpy(pkg.sender, my_username);
    strcpy(pkg.msg, TESTING_MSG);
    pkg.ctrl_signal = PRIVATE_CHAT;

    // Đặt trạng thái 'doing' về 0
   // doing = 0;

    // Gửi gói tin đến máy chủ
    send(client_socket, &pkg, sizeof(pkg), 0);
    printf("ccc\n");

    // Chờ đến khi trạng thái 'doing' thay đổi (khi nhận được phản hồi từ máy chủ)
    while (!doing);
    printf("dd\n");

    // Trả về trạng thái mới của 'doing'
    return doing;
    
}


// Hàm bắt đầu cuộc trò chuyện riêng với người dùng khác
int private_chat(int client_socket, char *receiver, char *msg)
{
    // Tạo một đối tượng Package để chứa thông điệp gửi
    Package pkg;

    // Đặt tên người nhận và người gửi vào gói tin
    strcpy(pkg.receiver, receiver);
    strcpy(pkg.sender, my_username);

    // Kiểm tra xem khóa công khai của người nhận đã có sẵn hay không
    printf("Kiểm tra khóa công khai\n");
    int check = check_public_key(client_socket, receiver);

    // Nếu khóa công khai chưa có sẵn, yêu cầu khóa công khai từ máy chủ và đợi nhận
    if (check == 0) {
        user_pub->public_key->exponent = 0;
        printf("Chưa có khóa công khai\n");
        while (!user_pub->public_key->exponent);
        printf("Nhận khóa công khai thành công\n");
    }

    // Hiển thị thông báo bắt đầu cuộc trò chuyện riêng
    printf("Bắt đầu trò chuyện với %s\n", receiver);

    // Đặt tín hiệu điều khiển để thông báo là cuộc trò chuyện riêng
    pkg.ctrl_signal = PRIVATE_CHAT;

    // Nếu tin nhắn là TESTING_MSG, sử dụng tin nhắn này, ngược lại, mã hóa tin nhắn và đặt vào gói tin
    if (strcmp(msg, TESTING_MSG) == 0)
        strcpy(pkg.msg, msg);
    else {
        // Mã hóa tin nhắn bằng khóa công khai của người nhận
        long long *encrypted = rsa_encrypt(msg, strlen(msg), user_pub->public_key);

        // Đặt tin nhắn đã mã hóa vào gói tin
        memset(pkg.encrypted_msg, '\0', sizeof(pkg.encrypted_msg));
        int i = 0;
        printf("Đã mã hóa!\n");
        for (i = 0; i < strlen(msg); i++) {
            pkg.encrypted_msg[i] = (long long)encrypted[i];
            printf("%lld ", (long long)pkg.encrypted_msg[i]);
        }
        printf("\n");
        i = 0;
        while (pkg.encrypted_msg[i] != 0) {
            printf("%lld\n", pkg.encrypted_msg[i]);
            i++;
        }
        printf("\n");
    }

    // Gửi gói tin chứa tin nhắn đến máy chủ
    send(client_socket, &pkg, sizeof(pkg), 0);
    printf("Đã gửi tin nhắn\n");

    
}


// Hàm bắt đầu cuộc trò chuyện nhóm
void chat_all(int client_socket)
{
    // Tạo một đối tượng Package để chứa thông điệp gửi
    Package pkg;

    // Đặt tín hiệu điều khiển để thông báo là cuộc trò chuyện nhóm
    pkg.ctrl_signal = CHAT_ALL;

    // Đặt tên người gửi vào gói tin
    strcpy(pkg.sender, my_username);

    // Khai báo một biến để lưu trữ tin nhắn từ người dùng
    char msg[MSG_SIZE];

    // Vòng lặp để nhập và gửi tin nhắn cho đến khi người dùng nhập tin nhắn trống
    while (1)
    {
        printf("Nhập tin nhắn (để trống để thoát khỏi cuộc trò chuyện nhóm):\n");
        fgets(msg, MSG_SIZE, stdin);
        msg[strlen(msg) - 1] = '\0';

        // Nếu người dùng nhập tin nhắn trống, thoát khỏi vòng lặp
        if (strlen(msg) == 0)
        {
            break;
        }

        // Sao chép tin nhắn vào gói tin
        strcpy(pkg.msg, msg);

        // Gửi gói tin chứa tin nhắn đến máy chủ
        send(client_socket, &pkg, sizeof(pkg), 0);

        
    }
}


//  xu ly lua chon trong group chat menu
// void group_chat_init(int client_socket)
// {
//     Package pkg;
//     pkg.ctrl_signal = GROUP_CHAT_INIT;
//     send(client_socket, &pkg, sizeof(pkg), 0);
//     // xu ly
//     int choice = 0;

//     while (1)
//     {
//         sleep(1);

//         group_chat_menu();
//         printf("Your choice: \n");
//         scanf("%d", &choice);
//         clear_stdin_buff();

//         switch (choice)
//         {
//         case 1:
//             show_group(client_socket);
//             break;
//         case 2:
//             new_group(client_socket);
//             break;
//         case 3:
//             join_group(client_socket);
//             break;
//         default:
//             return;
//         }
//     }
// }


char* group_msg_encrypt(char* msg, char* key) {

}

char* group_msg_decrypt(char* msg, char* key) {

}

// hien thi nhom hien tai
void show_group(int client_socket)
{
    Package pkg;
    pkg.ctrl_signal = SHOW_GROUP;
    send(client_socket, &pkg, sizeof(pkg), 0);
    // sleep(1);
}

// tao group moi
void new_group(int client_socket)
{
    Package pkg;
    pkg.ctrl_signal = NEW_GROUP;
    send(client_socket, &pkg, sizeof(pkg), 0);
}


// Hàm tham gia vào một nhóm
void join_group(int client_socket, char *group_name)
{
    // Tạo một đối tượng Package để chứa thông điệp gửi
    Package pkg;
    pkg.ctrl_signal = JOIN_GROUP;

    // Đặt tên người gửi và tên nhóm vào gói tin
    strcpy(pkg.sender, my_username);
    strcpy(pkg.msg, group_name);

    // Gửi gói tin chứa yêu cầu tham gia nhóm đến máy chủ
    send(client_socket, &pkg, sizeof(pkg), 0);
}


// moi ban vào group
void invite_friend(int client_socket, char *friend_username)
{
    // Tạo một đối tượng Package để chứa thông điệp gửi
    Package pkg;
    // Đặt tên người gửi, người nhận, và tin nhắn vào gói tin
    strcpy(pkg.sender, my_username);
    strcpy(pkg.receiver, friend_username);
    strcpy(pkg.msg, my_username);
    strcat(pkg.msg, " Added you to ");
    strcat(pkg.msg, curr_group_name);
    // Đặt tín hiệu điều khiển để thông báo là yêu cầu mời tham gia nhóm
    pkg.ctrl_signal = INVITE_FRIEND;
    // Đặt ID của nhóm vào gói tin
    pkg.group_id = curr_group_id;
    // Gửi gói tin chứa thông điệp mời tham gia nhóm đến máy chủ
    send(client_socket, &pkg, sizeof(pkg), 0);
}


// Hàm bắt đầu cuộc trò chuyện nhóm
void group_chat(int client_socket, char *msg)
{
    // Tạo một đối tượng Package để chứa thông điệp gửi
    Package pkg;
    pkg.ctrl_signal = GROUP_CHAT;

    // Đặt ID của nhóm, tên người gửi và tin nhắn vào gói tin
    pkg.group_id = curr_group_id;
    strcpy(pkg.sender, my_username);
    strcpy(pkg.msg, msg);

    // Lưu trữ tin nhắn vào lịch sử trò chuyện nhóm
    save_chat(&pkg);

    // Gửi gói tin chứa tin nhắn nhóm đến máy chủ
    send(client_socket, &pkg, sizeof(pkg), 0);
}


// Hàm hiển thị thông tin của một nhóm
void show_group_info(int client_socket)
{
    // Tạo một đối tượng Package để chứa yêu cầu hiển thị thông tin nhóm
    Package pkg;
    pkg.ctrl_signal = GROUP_INFO;

    // Đặt ID của nhóm vào gói tin
    pkg.group_id = curr_group_id;

    // Gửi yêu cầu hiển thị thông tin nhóm đến máy chủ
    send(client_socket, &pkg, sizeof(pkg), 0);
}

// Hàm rời khỏi một nhóm
void leave_group(int client_socket)
{
    // Tạo một đối tượng Package để chứa thông điệp gửi
    Package pkg;
    pkg.ctrl_signal = LEAVE_GROUP;

    // Đặt ID của nhóm và tên người gửi vào gói tin
    pkg.group_id = curr_group_id;
    strcpy(pkg.sender, my_username);

    // Gửi gói tin chứa yêu cầu rời nhóm đến máy chủ
    send(client_socket, &pkg, sizeof(pkg), 0);
}


// main
// int main()
// {
//     int client_socket = connect_to_server();
//     ask_server(client_socket);
//     return 0;
// }