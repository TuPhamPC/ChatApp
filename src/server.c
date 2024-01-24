#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

Active_user user[MAX_USER];
Group group[MAX_GROUP];
Account *acc_list;

Public_key_users pub[512];
int pubkey_count = 0;

// Tạo và cấu hình socket nghe kết nối mới
int create_listen_socket()
{

    int listen_socket;
    struct sockaddr_in server_addr;

    // Tạo socket nghe kết nối
    if ((listen_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        report_err(ERR_SOCKET_INIT);  // Báo lỗi nếu không thể khởi tạo socket
        exit(0);
    }

    // Thiết lập thông tin địa chỉ server
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    // Liên kết socket với địa chỉ và cổng
    if (bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        report_err(ERR_SOCKET_INIT);  // Báo lỗi nếu không thể liên kết socket
        exit(0);
    }

    // Nghe kết nối từ client với số lượng tối đa là MAX_USER
    if (listen(listen_socket, MAX_USER) < 0)
    {
        report_err(ERR_SOCKET_INIT);  // Báo lỗi nếu không thể lắng nghe kết nối
        exit(0);
    }

    return listen_socket;  // Trả về socket nghe kết nối đã được cấu hình
}


// Chấp nhận kết nối từ client và trả về socket mới
int accept_conn(int listen_socket)
{

    int conn_socket;
    struct sockaddr_in client_addr;
    int client_addr_size = sizeof(struct sockaddr);

    // Chấp nhận kết nối từ client và tạo socket mới để giao tiếp
    if ((conn_socket = accept(listen_socket, (struct sockaddr *)&client_addr, &client_addr_size)) < 0)
    {
        report_err(ERR_CONN_ACCEPT);  // Báo lỗi nếu không thể chấp nhận kết nối
        exit(0);
    }

    return conn_socket;  // Trả về socket mới đã được kết nối
}


// Hàm tạo server và chấp nhận kết nối từ client
void make_server()
{

    int listen_socket;

    // Đọc danh sách tài khoản từ file
    acc_list = read_account_list();

    // Tạo socket nghe kết nối
    listen_socket = create_listen_socket();

    // Khởi tạo thông tin người dùng và nhóm
    for (int i = 0; i < MAX_USER; i++)
    {
        user[i].socket = -1;
        for (int j = 0; j < MAX_GROUP; j++)
            user[i].group_id[j] = -1;
    }
    for (int i = 0; i < MAX_GROUP; i++)
    {
        for (int j = 0; j < MAX_USER; j++)
        {
            group[i].group_member[j].socket = -1;
        }
        group[i].curr_num = 0;
    }

    printf("Server created\n");

    // Chấp nhận và xử lý kết nối từ client
    while (1)
    {

        int conn_socket = accept_conn(listen_socket);

        // Tạo một luồng mới để xử lý kết nối
        pthread_t client_thr;
        if (pthread_create(&client_thr, NULL, pre_login_srv, (void *)&conn_socket) < 0)
        {
            report_err(ERR_CREATE_THREAD);  // Báo lỗi nếu không thể tạo luồng
            exit(0);
        }
        pthread_detach(client_thr);  // Cho phép luồng tự giải phóng tài nguyên khi hoàn thành
    }

    close(listen_socket);  // Đóng socket nghe kết nối
}


// Hàm xử lý trước khi đăng nhập của server trong một luồng riêng biệt
void *pre_login_srv(void *param)
{

    int conn_socket = *((int *)param);
    Package pkg;

    // Vòng lặp nhận và xử lý gói tin từ client
    while (1)
    {

        // Nhận gói tin từ client
        recv(conn_socket, &pkg, sizeof(pkg), 0);

        // Xử lý gói tin theo tín hiệu điều khiển
        switch (pkg.ctrl_signal)
        {
        case LOGIN_REQ:
            handle_login(conn_socket, acc_list);  // Xử lý yêu cầu đăng nhập
            break;
        case QUIT_REQ:
            close(conn_socket);  // Đóng socket kết nối
            printf("user quit\n");
            pthread_exit(NULL);  // Kết thúc luồng khi client thoát
        }
    }
}


// Xử lý yêu cầu đăng nhập từ client
void handle_login(int conn_socket, Account *acc_list)
{

    char username[USERNAME_SIZE];
    char password[PASSWORD_SIZE];
    Package pkg;
    Account *target_acc;
    int result;

    // Nhận tên đăng nhập từ client
    recv(conn_socket, &pkg, sizeof(pkg), 0);
    strcpy(username, pkg.msg);

    // Gửi xác nhận nhận tên đăng nhập thành công về client
    pkg.ctrl_signal = RECV_SUCC;
    send(conn_socket, &pkg, sizeof(pkg), 0);

    // Nhận mật khẩu từ client
    recv(conn_socket, &pkg, sizeof(pkg), 0);
    strcpy(password, pkg.msg);

    // In ra tên đăng nhập và mật khẩu nhận được từ client (để kiểm tra)
    printf("%s\n", username);
    printf("%s\n", password);

    // Tìm tài khoản trong danh sách
    target_acc = find_account(acc_list, username);

    // Kiểm tra kết quả đăng nhập
    if (target_acc)
    {
        if (target_acc->is_signed_in)
        {
            result = SIGNED_IN_ACC;  // Tài khoản đã đăng nhập từ trước
        }
        else
        {
            if (strcmp(target_acc->password, password) == 0)
            {
                result = LOGIN_SUCC;  // Đăng nhập thành công
            }
            else
            {
                result = INCORRECT_ACC;  // Sai mật khẩu
            }
        }
    }
    else
    {
        result = INCORRECT_ACC;  // Tài khoản không tồn tại
    }

    // Xử lý kết quả đăng nhập
    if (result == LOGIN_SUCC)
    {
        printf("login success\n");

        // Đánh dấu tài khoản đã đăng nhập
        target_acc->is_signed_in = 1;

        // Cập nhật thông tin người dùng và nhóm
        for (int i = 0; i < MAX_USER; i++)
        {
            if (user[i].socket < 0)
            {
                strcpy(user[i].username, username);
                user[i].socket = conn_socket;
                sv_update_port_group(&user[i], group);
                break;
            }
        }
    }
    else if (result == SIGNED_IN_ACC)
    {
        printf("already signed in acc\n");
    }
    else
    {
        printf("incorrect acc\n");
    }

    // Gửi kết quả đăng nhập về client
    pkg.ctrl_signal = result;
    send(conn_socket, &pkg, sizeof(pkg), 0);

    // Nếu đăng nhập thành công, thực hiện các thao tác sau đăng nhập
    if (result == LOGIN_SUCC)
        sv_user_use(conn_socket);
}

// Xử lý các hoạt động của người dùng sau khi đăng nhập
void sv_user_use(int conn_socket)
{

    Package pkg;
    int login = 1;
    while (login)
    {
        // Nhận gói tin từ client
        if (recv(conn_socket, &pkg, sizeof(pkg), 0) > 0) // printf("Receive from %d\n", conn_socket);
            printf("\n--------------\n%d chooses %d \n", conn_socket, pkg.ctrl_signal);
        
        // Xử lý gói tin theo tín hiệu điều khiển
        switch (pkg.ctrl_signal)
        {
        case PRIVATE_CHAT:
            sv_private_chat(conn_socket, &pkg);  // Xử lý tin nhắn riêng
            break;
        case SEND_PUBLIC_KEY:
            save_public_key(pkg.sender, pkg.msg);  // Lưu khóa công khai của người dùng
            break;

        case SEND_PUBLIC_KEY_REQ:
            send_public_key(conn_socket, pkg.receiver);  // Gửi yêu cầu gửi khóa công khai đến người khác
            break;

        case CHAT_ALL:
            sv_chat_all(conn_socket, &pkg);  // Xử lý tin nhắn chung
            break;

        case SHOW_USER:
            sv_active_user(conn_socket, &pkg);  // Hiển thị danh sách người dùng
            break;

        case LOG_OUT:
            login = 0;
            sv_logout(conn_socket, &pkg);  // Xử lý yêu cầu đăng xuất
            break;
        case GROUP_CHAT_INIT:
            sv_group_chat_init(conn_socket, &pkg);  // Khởi tạo cuộc trò chuyện nhóm
            break;
        case SHOW_GROUP:
            sv_show_group(conn_socket, &pkg);  // Hiển thị danh sách các nhóm
            break;
        case NEW_GROUP:
            sv_new_group(conn_socket, &pkg);  // Tạo mới một nhóm
            break;
        case JOIN_GROUP:
            sv_join_group(conn_socket, &pkg);  // Tham gia vào một nhóm
            break;
        case HANDEL_GROUP_MESS:
            // hien ra thong tin phong
            break;
        case INVITE_FRIEND:
            sv_invite_friend(conn_socket, &pkg);  // Mời bạn bè tham gia nhóm
            break;
        case GROUP_CHAT:
            sv_group_chat(conn_socket, &pkg);   // Thực hiện cuộc trò chuyện nhóm
            break;
        case GROUP_INFO:
            sv_show_group_info(conn_socket, &pkg);   // Hiển thị thông tin chi tiết về nhóm
            break;
        case LEAVE_GROUP:
            sv_leave_group(conn_socket, &pkg);   // Rời khỏi nhóm
            break;
        default:
            break;
        }
        printf("Done %d of %d\n", pkg.ctrl_signal, conn_socket);  //in ra thông báo về tín hiệu đã được xử lý.
    }
    
    // Thực hiện các bước khi người dùng đăng xuất
    int i = 0;
    for (i = 0; i < MAX_USER; i++)
    {
        if (user[i].socket == conn_socket)
        {
            Account *target_acc = find_account(acc_list, user[i].username);
            // Đánh dấu tài khoản đã đăng xuất
            target_acc->is_signed_in = 0;
            user[i].socket = -1;

            // Cập nhật thông tin nhóm khi người dùng đăng xuất
            for (int j = 0; j < MAX_GROUP; j++)
            {
                if (user[i].group_id[j] >= 0)
                {
                    int group_id = user[i].group_id[j];
                    int user_id_group = sv_search_id_user_group(group[group_id], user[i].username);
                    if (user_id_group >= 0)
                    {
                       // Cập nhật trạng thái socket của thành viên nhóm khi đăng xuất
                        group[group_id].group_member[user_id_group].socket = 0; // can cap nhat khi dang nhap lai
                    }
                    user[i].group_id[j] = -1;
                }
            }
            break;
        }
    }
    // Xóa khóa công khai của người dùng khỏi danh sách khi đăng xuất
    for(int j = 0; j < pubkey_count; j++) {
        if(strcmp(user[i].username, pub[j].username) == 0) {
            pub[j].public_key->exponent = 0;
            pub[j].public_key->modulus = 0;
            break;
        }

    }
}

// Gửi danh sách người dùng hoạt động về client
void sv_active_user(int conn_socket, Package *pkg)
{

    char user_list[MSG_SIZE] = {0};

    // Tạo danh sách người dùng hoạt động
    for (int i = 0; i < MAX_USER; i++)
    {
        if (user[i].socket > 0)
        {
            strcat(user_list, user[i].username);
            int len = strlen(user_list);

            // Thêm khoảng trắng để phân tách giữa các tên người dùng
            if (i < MAX_USER - 1 && user[i + 1].socket > 0)
            {
                user_list[len] = ' ';
            }
        }
    }

    // Sao chép danh sách người dùng vào trường msg của gói tin
    strcpy(pkg->msg, user_list);

    // Gửi gói tin chứa danh sách người dùng về client
    send(conn_socket, pkg, sizeof(*pkg), 0);
}



// Kiểm tra khóa công khai của người dùng và lưu vào struct Public_key_users
int check_public_key(Public_key_users* user_pub, char* username) {
    int check = 0;
    
    // Duyệt qua danh sách các khóa công khai
    for(int i = 0; i < pubkey_count; i++) {
        
        // Tìm khóa công khai của người dùng
        if(strcmp(pub[i].username, username) == 0) {
            // Kiểm tra khóa công khai hợp lệ
            if(pub[i].public_key->exponent == 0) 
                return 0;  // Không có khóa công khai hoặc đã bị thu hồi
            // Lưu khóa công khai vào struct Public_key_users
            user_pub->public_key->exponent = pub[i].public_key->exponent;
            user_pub->public_key->modulus = pub[i].public_key->modulus;
            strcpy(user_pub->username, username);
            check = 1;  // Đã tìm thấy và lưu khóa công khai
            break;
        }
    }
    return check;  // Trả về kết quả kiểm tra
}


// Gửi khóa công khai của người dùng đến một người dùng khác
void send_public_key(int client_socket, char* receiver) {
    Package pkg;

    // Sao chép tên người nhận vào gói tin
    strcpy(pkg.receiver, receiver);

    // Tạo struct để lưu khóa công khai của người nhận
    Public_key_users user_key[1];

    // Kiểm tra sự tồn tại và hợp lệ của khóa công khai của người nhận
    int check = check_public_key(user_key, receiver);

    // Nếu không tồn tại hoặc không hợp lệ, thông báo lỗi và gửi gói tin với tín hiệu lỗi
    if(check == 0) {
        printf("No public key of %s\n", receiver);
        pkg.ctrl_signal = ERR_INVALID_RECEIVER;
        send(client_socket, &pkg, sizeof(pkg), 0);
        return;
    }

    // Thiết lập tín hiệu điều khiển và gói tin chứa khóa công khai
    pkg.ctrl_signal = SEND_PUBLIC_KEY;
    memcpy(pkg.msg, &user_key->public_key->modulus, sizeof(user_key->public_key->modulus));
    memcpy(pkg.msg + sizeof user_key->public_key->modulus, &user_key->public_key->exponent, sizeof(user_key->public_key->exponent));

    // In thông tin khóa công khai trước khi gửi
    printf("Public Key of %s:\n Modulus: %lld\n Exponent: %lld\n", receiver, (long long)user_key->public_key->modulus, (long long)user_key->public_key->exponent);

    // Gửi gói tin chứa khóa công khai đến người nhận
    send(client_socket, &pkg, sizeof(pkg), 0);
    printf("Public key sent!\n\n");
}


// Lưu khóa công khai vào danh sách khóa công khai
void save_public_key(char* sender, char* msg) {
    int check = 0;
    int i;

    // Kiểm tra xem khóa công khai của người gửi đã tồn tại trong danh sách chưa
    for(i = 0; i < pubkey_count; i++) {
        if(strcmp(pub[i].username, sender) == 0) {
            // Nếu đã tồn tại, cập nhật khóa công khai
            pub[i].public_key->exponent = ((struct public_key_class*)msg)->exponent;
            pub[i].public_key->modulus = ((struct public_key_class*)msg)->modulus;
            check = 1;
            break;
        }
    }

    // Nếu chưa tồn tại, thêm mới vào danh sách khóa công khai
    if(check != 1) {
        i = pubkey_count;

        // Cập nhật thông tin mới vào danh sách
        pub[i].public_key->exponent = ((struct public_key_class*)msg)->exponent;
        pub[i].public_key->modulus = ((struct public_key_class*)msg)->modulus;
        strcpy(pub[i].username, sender);
        pubkey_count++;
    }

    // In thông tin khóa công khai sau khi lưu
    printf("Public Key of %s:\n Modulus: %lld\n Exponent: %lld\n\n", sender, (long long)pub[i].public_key->modulus, (long long)pub[i].public_key->exponent);
}


// Xử lý tin nhắn riêng giữa hai người dùng
void sv_private_chat(int conn_socket, Package *pkg)
{
    int i = 0;
    int recv_socket;

    // Tìm socket của người nhận dựa trên tên người nhận
    for (i = 0; i < MAX_USER; i++)
    {
        if (user[i].socket > 0)
            if(strcmp(pkg->receiver, user[i].username) == 0)
            {
                recv_socket = user[i].socket;
                break;
            }
    }

    // Nếu không tìm thấy người nhận, gửi thông báo lỗi về người gửi
    if (i == MAX_USER){
        pkg->ctrl_signal = ERR_INVALID_RECEIVER;
        send(conn_socket, pkg, sizeof(*pkg), 0);
        printf("sent err\n");
        return;
    }

    // Gửi thông báo thành công về người gửi
    pkg->ctrl_signal = MSG_SENT_SUCC;
    send(conn_socket, pkg, sizeof(*pkg), 0);
    printf("sent nor\n");

    // Nếu là tin nhắn kiểm tra khóa công khai, gửi khóa công khai đến cả hai người
    if(strcmp(pkg->msg, TESTING_MSG) == 0) {
        send_public_key(recv_socket, pkg->sender);
        send_public_key(conn_socket, pkg->receiver);
        return;
    }

    // Gửi tin nhắn riêng đến người nhận
    pkg->ctrl_signal = PRIVATE_CHAT;
    printf("%d: %s to %s: %s\n", pkg->ctrl_signal, pkg->sender, pkg->receiver, pkg->msg);
    send(recv_socket, pkg, sizeof(*pkg), 0);
    printf("Sent %d to %s\n", pkg->ctrl_signal, pkg->receiver);
}


// Xử lý tin nhắn chung gửi đến tất cả người dùng
void sv_chat_all(int conn_socket, Package *pkg)
{
    printf("%d: %s to all: %s\n", pkg->ctrl_signal, pkg->sender, pkg->msg);

    // Gửi tin nhắn đến tất cả người dùng hoạt động
    for (int i = 0; i < MAX_USER; i++)
    {
        if (user[i].socket > 0)
            send(user[i].socket, pkg, sizeof(*pkg), 0);
    }

    // Gửi thông báo thành công về người gửi
    pkg->ctrl_signal = MSG_SENT_SUCC;
    send(conn_socket, pkg, sizeof(*pkg), 0);
}



// Tìm kiếm thông tin người dùng dựa trên socket
int search_user(int conn_socket)
{
    for (int i = 0; i < MAX_USER; i++)
    {
        if (user[i].socket == conn_socket)
            return i;  // Trả về chỉ số của người dùng trong mảng
    }
    return -1;  // Không tìm thấy, trả về -1
}


// Tìm kiếm thông tin người dùng trong danh sách người dùng hoạt động
int sv_search_id_user(Active_user user[], char *user_name)
{
    int user_id = -1;

    for (int i = 0; i < MAX_USER; i++)
    {
        // So sánh tên người dùng và kiểm tra xem người dùng có đang hoạt động không
        if (strcmp(user[i].username, user_name) == 0 && user[i].socket >= 0)
        {
            user_id = i;
            return user_id;  // Trả về chỉ số của người dùng trong danh sách
        }
    }

    return -1;  // Không tìm thấy, trả về -1
}

// Tìm kiếm thông tin người dùng trong một nhóm
int sv_search_id_user_group(Group group, char *user_name)
{
    for (int i = 0; i < MAX_USER; i++)
    {
        // So sánh tên người dùng trong nhóm
        if (strcmp(group.group_member[i].username, user_name) == 0)
        {
            // Trả về chỉ số của người dùng trong nhóm
            return i;
        }
    }

    return -1;  // Không tìm thấy, trả về -1
}


// Khởi tạo chức năng chat nhóm cho người dùng
void sv_group_chat_init(int conn_socket, Package *pkg)
{
    // Gửi thông điệp mô tả chức năng chat nhóm về cho người dùng
    strcpy(pkg->msg, "CHUC NANG CHAT NHOM\n");
    send(conn_socket, pkg, sizeof(*pkg), 0);
}


// Hiển thị danh sách nhóm cho người dùng
void sv_show_group(int conn_socket, Package *pkg)
{
    // Tìm thông tin người dùng dựa trên socket
    int user_id = search_user(conn_socket);

    // Tạo danh sách chứa tên các nhóm mà người dùng đã tham gia
    char group_list[MSG_SIZE] = {0};

    // Duyệt qua các nhóm mà người dùng đã tham gia
    for (int i = 0; i < MAX_GROUP; i++)
    {
        if (user[user_id].group_id[i] >= 0)
        {
            int group_id = user[user_id].group_id[i];
            strcat(group_list, group[group_id].group_name);
            int len = strlen(group_list);

            // Thêm khoảng trắng để phân tách giữa các tên nhóm
            if (i < MAX_GROUP - 1 && user[user_id].group_id[i + 1] >= 0)
            {
                group_list[len] = ' ';
            }
        }
    }

    // Sao chép danh sách nhóm vào trường msg của gói tin
    strcpy(pkg->msg, group_list);

    // Gửi gói tin chứa danh sách nhóm về cho người dùng
    send(conn_socket, pkg, sizeof(*pkg), 0);
}

// new group

// Kiểm tra xem người dùng có thuộc nhóm không
int check_user_in_group(Active_user user, int group_id)
{
    for (int i = 0; i < MAX_GROUP; i++)
    {
        if (user.group_id[i] == group_id)
            return 1;  // Người dùng thuộc nhóm
    }
    return 0;  // Người dùng không thuộc nhóm
}

// Thêm người dùng vào nhóm
int sv_add_group_user(Active_user *user, int group_id)
{
    for (int i = 0; i < MAX_GROUP; i++)
    {
        if (user->group_id[i] < 0)
        {
            user->group_id[i] = group_id;
            return 1;  // Thêm thành công
        }
    }
    return 0;  // Không thể thêm do đạt đến số lượng nhóm tối đa
}


// Thêm người dùng vào một nhóm
int sv_add_user(Active_user user, Group *group)
{
    for (int i = 0; i < MAX_USER; i++)
    {
        if (group->group_member[i].socket < 0)
        {
            // Thêm thông tin người dùng vào nhóm
            group->group_member[i].socket = user.socket;
            strcpy(group->group_member[i].username, user.username);
            
            // Tăng số lượng thành viên của nhóm
            group->curr_num++;

            return i;  // Trả về chỉ số của người dùng trong nhóm
        }
    }
    return 0;  // Không thể thêm do đạt đến số lượng thành viên tối đa
}


// In danh sách thành viên của một nhóm
void print_members(Group group)
{
    printf("MEMBERS OF GROUP %s: \n", group.group_name);

    for (int i = 0; i < MAX_USER; i++)
    {
        if (group.group_member[i].socket > 0)
        {
            printf("%s\n", group.group_member[i].username);
        }
    }
}

// Tạo nhóm mới cho người dùng
void sv_new_group(int conn_socket, Package *pkg)
{
    // Tìm thông tin người dùng dựa trên socket
    int user_id = search_user(conn_socket);
    int group_id = -1;

    // Duyệt qua danh sách nhóm để tìm nhóm chưa có thành viên nào
    for (int i = 0; i < MAX_GROUP; i++)
    {
        if (group[i].curr_num == 0)
        {
            group_id = i;

            // Thêm người dùng vào nhóm và cập nhật thông tin nhóm
            sv_add_group_user(&user[user_id], group_id);
            sv_add_user(user[user_id], &group[i]);
            sprintf(group[i].group_name, "Group_%d", group_id);
            break;
        }
    }

    // Xóa bảng tin nhắn của nhóm
    drop_table(group_id);

    // Sao chép tên nhóm vào trường msg của gói tin
    strcpy(pkg->msg, group[group_id].group_name);

    // Cập nhật mã điều khiển của gói tin
    pkg->ctrl_signal = MSG_MAKE_GROUP_SUCC;

    // Gửi gói tin về cho người dùng
    send(conn_socket, pkg, sizeof(*pkg), 0);
}



// Tìm kiếm thông tin về nhóm dựa trên tên nhóm
int sv_search_id_group(Group group[], Active_user user, char *group_name)
{
    int group_id = -1;

    // Duyệt qua các nhóm mà người dùng đã tham gia
    for (int i = 0; i < MAX_GROUP; i++)
    {
        if (user.group_id[i] >= 0)
        {
            group_id = user.group_id[i];

            // So sánh tên nhóm để xác định có phải nhóm cần tìm không
            if (strcmp(group[group_id].group_name, group_name) == 0)
            {
                return group_id;  // Trả về chỉ số của nhóm trong danh sách
            }
        }
    }

    return -1;  // Không tìm thấy, trả về -1
}


// Xử lý yêu cầu tham gia nhóm từ phía người dùng
void sv_join_group(int conn_socket, Package *pkg)
{
    char group_name[GROUP_NAME_SIZE];
    int group_id = -1;
    int user_id = -1;

    // Tìm thông tin người dùng dựa trên socket
    user_id = search_user(conn_socket);

    // Sao chép tên nhóm từ gói tin
    strcpy(group_name, pkg->msg);

    // Tìm chỉ số của nhóm trong danh sách
    group_id = sv_search_id_group(group, user[user_id], group_name);

    // Kiểm tra xem nhóm có tồn tại không
    if (group_id >= 0)
    {
        printf("%s JOIN GROUP %s\n", pkg->sender, group[group_id].group_name);

        // Sao chép tên nhóm vào trường msg của gói tin
        strcpy(pkg->msg, group_name);

        // Cập nhật mã điều khiển và thông tin nhóm trong gói tin
        pkg->ctrl_signal = JOIN_GROUP_SUCC;
        pkg->group_id = group_id;

        // Gửi gói tin về cho người dùng
        send(conn_socket, pkg, sizeof(*pkg), 0);
    }
    else
    {
        // Nếu nhóm không tồn tại, gửi thông báo lỗi về cho người dùng
        pkg->ctrl_signal = ERR_GROUP_NOT_FOUND;
        send(conn_socket, pkg, sizeof(*pkg), 0);
    }
}


// Xử lý yêu cầu mời thêm bạn vào nhóm từ phía người dùng
void sv_invite_friend(int conn_socket, Package *pkg)
{
    char friend_name[USERNAME_SIZE];
    int user_id = search_user(conn_socket);
    int friend_id;
    int group_id;

    // Lấy thông tin nhóm và bạn bè từ gói tin
    group_id = pkg->group_id;
    strcpy(friend_name, pkg->receiver);

    // Tìm thông tin về người được mời (bạn bè) trong danh sách người dùng
    friend_id = sv_search_id_user(user, friend_name);

    if (friend_id >= 0)
    {
        // Kiểm tra các điều kiện để mời bạn vào nhóm
        if (friend_id == user_id)
        {
            // Nếu người mời và người được mời là cùng một người, thông báo lỗi
            pkg->ctrl_signal = ERR_INVITE_MYSELF;
            send(conn_socket, pkg, sizeof(*pkg), 0);
            return;
        }
        else if (group[group_id].curr_num > MAX_USER - 1)
        {
            // Nếu nhóm đã đầy, thông báo lỗi
            pkg->ctrl_signal = ERR_FULL_MEMBERS;
            send(conn_socket, pkg, sizeof(*pkg), 0);
            return;
        }
        else if (check_user_in_group(user[friend_id], group_id))
        {
            // Nếu người được mời đã là thành viên của nhóm, thông báo lỗi
            pkg->ctrl_signal = ERR_ALREADY_MEMBER;
            send(conn_socket, pkg, sizeof(*pkg), 0);
            return;
        }
        else // Nếu mọi điều kiện đều đúng, thêm bạn bè vào nhóm
        {
            // Sao chép tên nhóm vào trường msg của gói tin
            strcpy(pkg->msg, group[group_id].group_name);

            // Gửi yêu cầu mời thêm bạn bè vào nhóm cho người được mời
            send(user[friend_id].socket, pkg, sizeof(*pkg), 0);

            printf("%s add %s to %s\n", user[user_id].username, user[friend_id].username, group[group_id].group_name);

            // Thêm bạn bè vào nhóm và cập nhật thông tin nhóm và người dùng
            sv_add_group_user(&user[friend_id], group_id);
            sv_add_user(user[friend_id], &group[group_id]);

            // Cập nhật thông tin gói tin và gửi về cho người mời
            pkg->ctrl_signal = INVITE_FRIEND_SUCC;
            send(conn_socket, pkg, sizeof(*pkg), 0);

            // Gửi thông báo đến tất cả thành viên trong nhóm
            memset(pkg->sender, '\0', sizeof(pkg->sender));
            strcpy(pkg->sender, SERVER_SYSTEM_USERNAME);
            memset(pkg->msg, '\0', sizeof(pkg->msg));
            sprintf(pkg->msg, "\"%s\" đã thêm \"%s\" vào nhóm.", user[user_id].username, user[friend_id].username);
            pkg->ctrl_signal = GROUP_CHAT;
            sv_group_chat(conn_socket, pkg);
        }
    }
    else
    {
        // Nếu không tìm thấy người được mời, thông báo lỗi
        pkg->ctrl_signal = ERR_USER_NOT_FOUND;
        send(conn_socket, pkg, sizeof(*pkg), 0);
        return;
    }
}


// Gửi tin nhắn trong nhóm
void sv_group_chat(int conn_socket, Package *pkg)
{
    int group_id = pkg->group_id;
    
    // Gửi tin nhắn đến tất cả thành viên trong nhóm
    for (int i = 0; i < MAX_USER; i++)
    {
        if (group[group_id].group_member[i].socket > 0 && group[group_id].group_member[i].socket != conn_socket)
        {
            send(group[group_id].group_member[i].socket, pkg, sizeof(*pkg), 0);
        }
    }
    
    // Cập nhật mã điều khiển và gửi thông báo về cho người gửi
    pkg->ctrl_signal = MSG_SENT_SUCC;
    send(conn_socket, pkg, sizeof(*pkg), 0);
}


// Hiển thị thông tin của nhóm
void sv_show_group_info(int conn_socket, Package *pkg)
{
    // Gửi giao diện thông tin (bắt đầu gửi thông tin)
    pkg->ctrl_signal = SHOW_GROUP_INFO_START;
    send(conn_socket, pkg, sizeof(*pkg), 0);

    int group_id = pkg->group_id;

    // Hiển thị tên nhóm
    printf("Group name: %s\n", group[group_id].group_name);
    strcpy(pkg->msg, group[group_id].group_name);
    pkg->ctrl_signal = SHOW_GROUP_NAME;
    send(conn_socket, pkg, sizeof(*pkg), 0);

    // Hiển thị thành viên trong nhóm
    print_members(group[group_id]);

    // Gửi số lượng thành viên
    sprintf(pkg->msg, "%d", group[group_id].curr_num);
    pkg->ctrl_signal = SHOW_GROUP_MEM_NUMBER;
    send(conn_socket, pkg, sizeof(*pkg), 0);

    // Gửi tên các thành viên trong nhóm
    for (int i = 0; i < MAX_USER; i++)
    {
        if (group[group_id].group_member[i].socket >= 0)
        {
            strcpy(pkg->msg, group[group_id].group_member[i].username);
            pkg->ctrl_signal = SHOW_GROUP_MEM_USERNAME;
            send(conn_socket, pkg, sizeof(*pkg), 0);
        }
    }

    // Gửi giao diện thông tin (đã gửi xong thông tin)
    pkg->ctrl_signal = SHOW_GROUP_INFO_END;
    send(conn_socket, pkg, sizeof(*pkg), 0);
}


// Rời khỏi nhóm
void sv_leave_group(int conn_socket, Package *pkg)
{
    int group_id = pkg->group_id;
    int user_id = search_user(conn_socket);
    int i = 0;

    // Tìm thành viên trong nhóm và đánh dấu rời nhóm
    for (i = 0; i < MAX_USER; i++)
    {
        Member mem = group[group_id].group_member[i];
        if (strcmp(mem.username, user[user_id].username) == 0)
        {
            group[group_id].group_member[i].socket = -1;
            strcpy(group[group_id].group_member[i].username, NULL_STRING);
            group[group_id].curr_num--;

            // Nếu người rời là thành viên cuối cùng, có thể xóa nhóm
            if (group[group_id].curr_num == 0)
            {
                drop_table(group_id);
            }

            // Gửi thông báo rời nhóm thành công cho người gửi
            strcpy(pkg->msg, "LEAVE GROUP SUCCESS: ");
            strcat(pkg->msg, group[group_id].group_name);
            pkg->ctrl_signal = LEAVE_GROUP_SUCC;
            send(conn_socket, pkg, sizeof(*pkg), 0);

            // Gửi thông báo đến tất cả thành viên trong nhóm
            memset(pkg->sender, '\0', sizeof(pkg->sender));
            strcpy(pkg->sender, SERVER_SYSTEM_USERNAME);
            memset(pkg->msg, '\0', sizeof(pkg->msg));
            sprintf(pkg->msg, "Người dùng \"%s\" đã rời nhóm và không còn là thành viên của nhóm.", user[user_id].username);
            pkg->ctrl_signal = GROUP_CHAT;
            sv_group_chat(conn_socket, pkg);
            break;
        }
    }
}


// Hàm đánh dấu người dùng rời khỏi nhóm
int sv_leave_group_user(Active_user *user, int group_id)
{
    for (int i = 0; i < MAX_GROUP; i++)
    {
        if (user->group_id[i] == group_id)
        {
            user->group_id[i] = -1;
            return 1; // Người dùng đã rời khỏi nhóm
        }
    }
    return 0; // Người dùng không nằm trong nhóm
}


// Hàm cập nhật thông tin cổng kết nối của người dùng trong các nhóm mà người dùng tham gia
void sv_update_port_group(Active_user *user, Group *group)
{
    int i = 0;
    int user_id_port;
    
    // Duyệt qua tất cả các nhóm
    for (i = 0; i < MAX_GROUP; i++)
    {
        // Tìm xem người dùng có là thành viên của nhóm không
        user_id_port = sv_search_id_user_group(group[i], user->username);
        
        if (user_id_port >= 0)
        {
            // Cập nhật thông tin cổng kết nối và thêm người dùng vào nhóm
            sv_add_group_user(user, i);
            group[i].group_member[user_id_port].socket = user->socket;
        }
    }
}


// Hàm xử lý việc đăng xuất người dùng
void sv_logout(int conn_socket, Package *pkg)
{
    printf("%d logout\n", conn_socket);
    
    // Gửi thông báo đăng xuất cho người dùng
    pkg->ctrl_signal = LOG_OUT;
    send(conn_socket, pkg, sizeof(*pkg), 0);
}


// main
int main()
{
    make_server();
    return 0;
}