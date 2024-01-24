#include "client.h"
#include "client_gui.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

Package pkg;
GMutex ui_mutex;
int is_done;

sqlite3 *database;
char **resultp;
int nrow, ncolumn;

//* ----------------------- SIGNAL HANDLERS -----------------------
// Xử lý sự kiện khi nút đăng nhập được nhấn trong giao diện GTK
void on_login_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);
    int result;

    // Lấy tên đăng nhập và mật khẩu từ các ô nhập liệu
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(login_acc_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(login_pwd_entry));

    // Gửi yêu cầu đăng nhập đến máy chủ và kiểm tra kết quả
    pkg.ctrl_signal = LOGIN_REQ;
    send(client_socket, &pkg, sizeof(pkg), 0);
    result = login(client_socket, (char *) username, (char *) password);

    // Kiểm tra kết quả đăng nhập và thực hiện các hành động tương ứng
    if (result == LOGIN_SUCC) {
        // Nếu đăng nhập thành công, đóng cửa sổ đăng nhập và hiển thị cửa sổ chính
        gtk_widget_destroy(GTK_WIDGET(login_window));
        show_main_window((int *) data);
    } else if (result == INCORRECT_ACC) {
        // Nếu thông tin đăng nhập không chính xác, hiển thị thông báo lỗi
        notif_dialog(GTK_WINDOW(login_window), INCORRECT_ACC_NOTIF);
    } else {
        // Nếu tài khoản đã đăng nhập ở nơi khác, hiển thị thông báo lỗi
        notif_dialog(GTK_WINDOW(login_window), SIGNED_IN_ACC_NOTIF);
    }
}


// Xử lý sự kiện khi nút thoát đăng nhập được nhấn trong giao diện GTK
void on_login_exit_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Gửi yêu cầu thoát ứng dụng đến máy chủ và đóng kết nối
    pkg.ctrl_signal = QUIT_REQ;
    send(client_socket, &pkg, sizeof(pkg), 0);
    close(client_socket);

    // Gỡ bỏ khóa mutex đã sử dụng trong giao diện người dùng
    g_mutex_clear(&ui_mutex);

    // Đóng cửa sổ đăng nhập
    gtk_widget_destroy(GTK_WIDGET(login_window));
}


// Xử lý sự kiện khi nút làm mới danh sách được nhấn trong cửa sổ chính của giao diện GTK
void on_refresh_list_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Gửi yêu cầu hiển thị danh sách người dùng và nhóm đến máy chủ
    see_active_user(client_socket);
    show_group(client_socket);
}

// Xử lý sự kiện khi nút đăng xuất được nhấn trong cửa sổ chính của giao diện GTK
void on_logout_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Gửi yêu cầu đăng xuất đến máy chủ và đặt lại các biến global
    pkg.ctrl_signal = LOG_OUT;
    send(client_socket, &pkg, sizeof(pkg), 0);
    strcpy(my_username, NULL_STRING);
    strcpy(curr_group_name, NULL_STRING);
    curr_group_id = -1;

    // Đóng cửa sổ chính và hiển thị cửa sổ đăng nhập
    gtk_widget_destroy(GTK_WIDGET(main_window));
    show_login_window((int *) data);
}


// Xử lý sự kiện khi nút trò chuyện riêng được nhấn trong cửa sổ chính của giao diện GTK
void on_private_chat_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Hiển thị hộp thoại nhập tên người nhận
    show_receiver_username_dialog((int *) data);
}


// Xử lý sự kiện khi nút trò chuyện nhóm được nhấn trong cửa sổ chính của giao diện GTK
void on_group_chat_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Hiển thị hộp thoại nhập tên nhóm để tham gia
    show_join_group_dialog((int *) data);
}


// Xử lý sự kiện khi nút mời vào nhóm được nhấn trong cửa sổ chính của giao diện GTK
void on_group_invite_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Kiểm tra xem người dùng đã tham gia vào một nhóm nào đó chưa
    if (curr_group_id != -1) {
        // Nếu đã tham gia nhóm, hiển thị hộp thoại mời vào nhóm
        show_invite_to_group_dialog((int *) data);
    } else {
        // Nếu chưa tham gia nhóm, hiển thị thông báo lỗi
        notif_dialog(GTK_WINDOW(main_window), NOT_IN_GROUP_ROOM_NOTIF);
    }
}


// Xử lý sự kiện khi nút thông tin nhóm được nhấn trong cửa sổ chính của giao diện GTK
void on_group_info_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Kiểm tra xem người dùng đã tham gia vào một nhóm nào đó chưa
    if (curr_group_id != -1) {
        // Nếu đã tham gia nhóm, hiển thị thông tin nhóm
        show_group_info(client_socket);
    } else {
        // Nếu chưa tham gia nhóm, hiển thị thông báo lỗi
        notif_dialog(GTK_WINDOW(main_window), NOT_IN_GROUP_ROOM_NOTIF);
    }
}


// Xử lý sự kiện khi nút rời khỏi nhóm được nhấn trong cửa sổ chính của giao diện GTK
void on_group_leave_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Kiểm tra xem người dùng đã tham gia vào một nhóm nào đó chưa
    if (curr_group_id != -1) {
        // Nếu đã tham gia nhóm, thực hiện hành động rời khỏi nhóm
        leave_group(client_socket);
    } else {
        // Nếu chưa tham gia nhóm, hiển thị thông báo lỗi
        notif_dialog(GTK_WINDOW(main_window), NOT_IN_GROUP_ROOM_NOTIF);
    }
}


// Xử lý sự kiện khi nút gửi tin nhắn được nhấn trong cửa sổ chat của giao diện GTK
void on_send_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Lấy tên người nhận và tin nhắn từ các widget trong giao diện
    const gchar *receiver = gtk_label_get_text(GTK_LABEL(cur_chat_label));
    const gchar *message = gtk_entry_get_text(GTK_ENTRY(send_entry));

    // Kiểm tra xem người dùng đang chat nhóm hay riêng tư
    if (curr_group_id != -1) {
        // Nếu đang chat nhóm, gửi tin nhắn nhóm đến máy chủ
        group_chat(client_socket, (char *) message);
    } else {
        // Nếu đang chat riêng tư, kiểm tra và gửi tin nhắn riêng tư đến máy chủ
        int res = check_receiver(client_socket, (char *) receiver);
        if (res == ERR_INVALID_RECEIVER) return;
        private_chat(client_socket, (char *) receiver, (char *) message);

        // Hiển thị tin nhắn đã gửi lên ô chat
        insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), MYSELF_INDICATOR);
        insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), SPLITER);
        insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), (gchar *) message);
        insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), NEWLINE);

        // Cuộn ô chat đến cuối cùng để hiển thị tin nhắn mới
        while (gtk_events_pending()) gtk_main_iteration();
        scroll_window_to_bottom(GTK_SCROLLED_WINDOW(recv_msg_sw));
    }

    // Xóa nội dung của ô nhập tin nhắn sau khi đã gửi
    GtkEntryBuffer *entry_buffer;
    entry_buffer = gtk_entry_get_buffer(GTK_ENTRY(send_entry));
    gtk_entry_buffer_delete_text(entry_buffer, 0, -1);
}


// Xử lý sự kiện khi nút xác nhận tên người nhận được nhấn trong hộp thoại nhập tên người nhận của giao diện GTK
void on_receiver_username_confirm_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Lấy tên người nhận từ ô nhập liệu
    const gchar *receiver = gtk_entry_get_text(GTK_ENTRY(receiver_username_entry));

    // Kiểm tra và xử lý tên người nhận
    check_receiver(client_socket, (char *) receiver);

    // Đóng hộp thoại nhập tên người nhận
    gtk_widget_destroy(receiver_username_dialog);
}


// Xử lý sự kiện khi nút tạo nhóm được nhấn trong hộp thoại tham gia nhóm của giao diện GTK
void on_join_group_create_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Tạo một nhóm mới và hiển thị danh sách nhóm
    new_group(client_socket);
    show_group(client_socket);
}


// Xử lý sự kiện khi nút tham gia nhóm được nhấn trong hộp thoại tham gia nhóm của giao diện GTK
void on_join_group_join_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Lấy tên nhóm từ ô nhập liệu và thực hiện hành động tham gia nhóm
    const gchar *group_name = gtk_entry_get_text(GTK_ENTRY(join_group_entry));
    join_group(client_socket, (char *) group_name);

    // Đóng hộp thoại tham gia nhóm
    gtk_widget_destroy(join_group_dialog);
}


// Xử lý sự kiện khi nút xác nhận mời vào nhóm được nhấn trong hộp thoại mời vào nhóm của giao diện GTK
void on_invite_to_group_confirm_btn_clicked(GtkButton *btn, gpointer data) {

    // Lấy thông số kết nối đến máy chủ từ con trỏ data
    int client_socket = *((int *) data);

    // Lấy tên người được mời từ ô nhập liệu và thực hiện hành động mời vào nhóm
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(invite_to_group_entry));
    invite_friend(client_socket, (char *) username);

    // Đóng hộp thoại mời vào nhóm
    gtk_widget_destroy(invite_to_group_dialog);
}


// Hàm hiển thị lịch sử trò chuyện
void view_chat_history()
{
    // Tạo một đối tượng Package để chứa yêu cầu xem lịch sử trò chuyện
    Package pkg;
    pkg.group_id = curr_group_id;
    strcpy(pkg.sender, my_username);

    // Gửi yêu cầu xem lịch sử trò chuyện đến máy chủ
    see_chat(&pkg);
}


// Hàm xem lịch sử trò chuyện
void see_chat(Package *pkg)
{
    // Tạo hoặc mở cơ sở dữ liệu SQLite
    database = Create_room_sqlite(pkg);
    int ret;
    char *errmsg = NULL;
    char buf[MAX_SQL_SIZE] = "CREATE TABLE IF NOT EXISTS chat(time TEXT, sender TEXT, message TEXT)";
    ret = sqlite3_exec(database, buf, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK)
    {
        printf("Open table failed\n");
        return;
    }

    // Xem lịch sử trò chuyện từ cơ sở dữ liệu
    resultp = NULL;
    char *sq1 = "select * from chat";
    ret = sqlite3_get_table(database, sq1, &resultp, &nrow, &ncolumn, &errmsg);
    if (ret != SQLITE_OK)
    {
        printf("Database operation failed\n");
        printf("sqlite3_get_table: %s\n", errmsg);
    }

    // sqlite3_free_table(resultp);

    //sqlite3_close(database);
}


// Hàm cuộn cửa sổ đến cuối cùng
void scroll_window_to_bottom(GtkScrolledWindow *sw) {

    // Lấy thanh điều chỉnh dọc của cửa sổ cuộn
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(sw);
    
    // Lấy giá trị cao nhất và kích thước trang của thanh điều chỉnh
    double upper = gtk_adjustment_get_upper(vadj);
    double page_size = gtk_adjustment_get_page_size(vadj);
    
    // Đặt giá trị thanh điều chỉnh để cuộn cửa sổ đến cuối cùng
    gtk_adjustment_set_value(vadj, upper - page_size);
}


// Hàm chèn văn bản vào TextView
void insert_to_textview(GtkTextView *tv, gchar *text) {

    // Lấy bộ đệm và vị trí cuối cùng trong TextView
    GtkTextBuffer *buffer;
    GtkTextIter end_iter;

    buffer = gtk_text_view_get_buffer(tv);
    gtk_text_buffer_get_end_iter(buffer, &end_iter);

    // Chèn văn bản vào TextView
    gtk_text_buffer_insert(buffer, &end_iter, text, -1);
}


// Hàm xóa nội dung của TextView
void delete_textview_content(GtkTextView *tv) {
    
    // Lấy bộ đệm và vị trí bắt đầu và cuối cùng trong TextView
    GtkTextBuffer *buffer;
    GtkTextIter start_iter;
    GtkTextIter end_iter;

    buffer = gtk_text_view_get_buffer(tv);
    gtk_text_buffer_get_start_iter(buffer, &start_iter);
    gtk_text_buffer_get_end_iter(buffer, &end_iter);

    // Xóa nội dung từ vị trí bắt đầu đến cuối cùng trong TextView
    gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
}

// Hàm xóa dòng cuối cùng trong TextView
void delete_lastline_textview(GtkTextView *tv) {

    // Lấy bộ đệm và vị trí bắt đầu của dòng cuối cùng và cuối cùng trong TextView
    GtkTextBuffer *buffer;
    GtkTextIter last_line_start_iter, end_iter;
    gint line_count;

    buffer = gtk_text_view_get_buffer(tv);
    line_count = gtk_text_buffer_get_line_count(buffer);
    gtk_text_buffer_get_end_iter(buffer, &end_iter);

    // Lấy vị trí bắt đầu của dòng cuối cùng
    gtk_text_buffer_get_iter_at_line(buffer, &last_line_start_iter, line_count - 2);

    // Xóa nội dung từ vị trí bắt đầu của dòng cuối cùng đến cuối cùng trong TextView
    gtk_text_buffer_delete(buffer, &last_line_start_iter, &end_iter);
}


// Hàm hiển thị hộp thoại thông báo
void notif_dialog(GtkWindow *parent, gchar *message) {

    GtkWidget *dialog, *label, *content_area;
    GtkDialogFlags flags;

    // Tạo các widget
    flags = GTK_DIALOG_DESTROY_WITH_PARENT;
    dialog = gtk_dialog_new_with_buttons("Thông Báo", parent, flags, "OK", GTK_RESPONSE_NONE, NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG (dialog));
    
    label = gtk_label_new(message);
    gtk_widget_set_margin_top(label, 20);
    gtk_widget_set_margin_bottom(label, 20);
    gtk_widget_set_margin_start(label, 10);
    gtk_widget_set_margin_end(label, 10);

    // Kết nối sự kiện để đóng hộp thoại khi người dùng bấm nút OK
    g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);

    // Thêm label vào hộp thoại và hiển thị tất cả các thành phần
    gtk_container_add (GTK_CONTAINER (content_area), label);
    gtk_widget_show_all (dialog);
}


// Hàm hiển thị cửa sổ đăng nhập
void show_login_window(int *client_socket_pt) {

    // Load giao diện đăng nhập từ tệp tin Glade
    builder = gtk_builder_new_from_file("../views/login_window.glade");

    // Lấy các widget trong cửa sổ đăng nhập
    login_window = GTK_WIDGET(gtk_builder_get_object(builder, "login_window"));
    login_fixed = GTK_WIDGET(gtk_builder_get_object(builder, "login_fixed"));
    login_acc_entry = GTK_WIDGET(gtk_builder_get_object(builder, "login_acc_entry"));
    login_pwd_entry = GTK_WIDGET(gtk_builder_get_object(builder, "login_pwd_entry"));
    login_btn = GTK_WIDGET(gtk_builder_get_object(builder, "login_btn"));
    signup_btn = GTK_WIDGET(gtk_builder_get_object(builder, "signup_btn"));
    login_exit_btn = GTK_WIDGET(gtk_builder_get_object(builder, "login_exit_btn"));

    // Kết nối các xử lý tín hiệu cho các nút và sự kiện đóng cửa sổ
    g_signal_connect(login_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(login_btn, "clicked", G_CALLBACK(on_login_btn_clicked), client_socket_pt);
    g_signal_connect(login_exit_btn, "clicked", G_CALLBACK(on_login_exit_btn_clicked), client_socket_pt);

    // Hiển thị cửa sổ đăng nhập và đợi cho các tín hiệu được phát ra
    gtk_widget_show(login_window);
    gtk_main();
}


// Hàm hiển thị cửa sổ chính
void show_main_window(int *client_socket_pt) {

    // Load giao diện chính từ tệp tin Glade
    builder = gtk_builder_new_from_file("../views/main_window.glade");

    // Lấy các widget trong cửa sổ chính
    main_window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    main_fixed = GTK_WIDGET(gtk_builder_get_object(builder, "main_fixed"));
    online_sw = GTK_WIDGET(gtk_builder_get_object(builder, "online_sw"));
    online_tv = GTK_WIDGET(gtk_builder_get_object(builder, "online_tv"));
    group_sw = GTK_WIDGET(gtk_builder_get_object(builder, "group_sw"));
    group_tv = GTK_WIDGET(gtk_builder_get_object(builder, "group_tv"));
    private_chat_btn = GTK_WIDGET(gtk_builder_get_object(builder, "private_chat_btn"));
    group_chat_btn = GTK_WIDGET(gtk_builder_get_object(builder, "group_chat_btn"));
    refresh_list_btn = GTK_WIDGET(gtk_builder_get_object(builder, "refresh_list_btn"));
    cur_user_label = GTK_WIDGET(gtk_builder_get_object(builder, "cur_user_label"));
    logout_btn = GTK_WIDGET(gtk_builder_get_object(builder, "logout_btn"));
    group_invite_btn = GTK_WIDGET(gtk_builder_get_object(builder, "group_invite_btn"));
    group_info_btn = GTK_WIDGET(gtk_builder_get_object(builder, "group_info_btn"));
    group_leave_btn = GTK_WIDGET(gtk_builder_get_object(builder, "group_leave_btn"));
    cur_chat_label = GTK_WIDGET(gtk_builder_get_object(builder, "cur_chat_label"));
    recv_msg_sw = GTK_WIDGET(gtk_builder_get_object(builder, "recv_msg_sw"));
    recv_msg_tv = GTK_WIDGET(gtk_builder_get_object(builder, "recv_msg_tv"));
    send_entry = GTK_WIDGET(gtk_builder_get_object(builder, "send_entry"));
    send_btn = GTK_WIDGET(gtk_builder_get_object(builder, "send_btn"));

    // Kết nối các xử lý tín hiệu cho các nút và sự kiện đóng cửa sổ
    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(logout_btn, "clicked", G_CALLBACK(on_logout_btn_clicked), client_socket_pt);
    g_signal_connect(refresh_list_btn, "clicked", G_CALLBACK(on_refresh_list_btn_clicked), client_socket_pt);
    g_signal_connect(private_chat_btn, "clicked", G_CALLBACK(on_private_chat_btn_clicked), client_socket_pt);
    g_signal_connect(group_chat_btn, "clicked", G_CALLBACK(on_group_chat_btn_clicked), client_socket_pt);
    g_signal_connect(send_btn, "clicked", G_CALLBACK(on_send_btn_clicked), client_socket_pt);
    g_signal_connect(group_invite_btn, "clicked", G_CALLBACK(on_group_invite_btn_clicked), client_socket_pt);
    g_signal_connect(group_info_btn, "clicked", G_CALLBACK(on_group_info_btn_clicked), client_socket_pt);
    g_signal_connect(group_leave_btn, "clicked", G_CALLBACK(on_group_leave_btn_clicked), client_socket_pt);

    // Tạo luồng đọc
    g_thread_new("recv_handler", recv_handler, client_socket_pt);

    // Khởi tạo các hành động
    // Tin nhắn chào
    char hello_cur_user_msg[USERNAME_SIZE + 7] = {0};
    strcpy(hello_cur_user_msg, HELLO_USER_MSG);
    strcat(hello_cur_user_msg, my_username);
    gtk_label_set_text(GTK_LABEL(cur_user_label), hello_cur_user_msg);
    // Cur chat mặc định
    gtk_label_set_text(GTK_LABEL(cur_chat_label), DEFAULT_CUR_CHAT_LABEL);
    // Hiển thị người dùng đang online
    see_active_user(*client_socket_pt);
    // Hiển thị các nhóm mà người dùng tham gia
    show_group(*client_socket_pt);
    // Làm cho TextView không thể chỉnh sửa
    gtk_text_view_set_editable(GTK_TEXT_VIEW(online_tv), FALSE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(group_tv), FALSE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(recv_msg_tv), FALSE);

    // Hiển thị cửa sổ chính và đợi cho các tín hiệu được phát ra
    gtk_widget_show(main_window);
    gtk_main();
}


// Hàm hiển thị hộp thoại nhập tên người nhận
void show_receiver_username_dialog(int *client_socket_pt) {

    builder = gtk_builder_new_from_file("../views/receiver_username_dialog.glade");

    receiver_username_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "receiver_username_dialog"));
    receiver_username_box = GTK_WIDGET(gtk_builder_get_object(builder, "receiver_username_box"));
    receiver_username_entry = GTK_WIDGET(gtk_builder_get_object(builder, "receiver_username_entry"));
    receiver_username_btn_box = GTK_WIDGET(gtk_builder_get_object(builder, "receiver_username_btn_box"));
    receiver_username_confirm_btn = GTK_WIDGET(gtk_builder_get_object(builder, "receiver_username_confirm_btn"));
    receiver_username_exit_btn = GTK_WIDGET(gtk_builder_get_object(builder, "receiver_username_exit_btn"));

    // Kết nối sự kiện để đóng hộp thoại khi cửa sổ chính bị đóng
    g_signal_connect_swapped(main_window, "destroy", G_CALLBACK(gtk_widget_destroy), receiver_username_dialog);
    // Kết nối sự kiện xác nhận và đóng hộp thoại khi người dùng nhấn nút tương ứng
    g_signal_connect(receiver_username_confirm_btn, "clicked", G_CALLBACK(on_receiver_username_confirm_btn_clicked), client_socket_pt);
    g_signal_connect_swapped(receiver_username_exit_btn, "clicked", G_CALLBACK(gtk_widget_destroy), receiver_username_dialog);

    // Hiển thị tất cả các thành phần trong hộp thoại
    gtk_widget_show_all(receiver_username_dialog);
}


// Hàm hiển thị hộp thoại tham gia nhóm
void show_join_group_dialog(int *client_socket_pt) {

    builder = gtk_builder_new_from_file("../views/join_group_dialog.glade");

    join_group_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "join_group_dialog"));
    join_group_box = GTK_WIDGET(gtk_builder_get_object(builder, "join_group_box"));
    join_group_entry = GTK_WIDGET(gtk_builder_get_object(builder, "join_group_entry"));
    join_group_btn_box = GTK_WIDGET(gtk_builder_get_object(builder, "join_group_btn_box"));
    join_group_join_btn = GTK_WIDGET(gtk_builder_get_object(builder, "join_group_join_btn"));
    join_group_create_btn = GTK_WIDGET(gtk_builder_get_object(builder, "join_group_create_btn"));
    join_group_exit_btn = GTK_WIDGET(gtk_builder_get_object(builder, "join_group_exit_btn"));

    // Kết nối sự kiện để đóng hộp thoại khi cửa sổ chính bị đóng
    g_signal_connect_swapped(main_window, "destroy", G_CALLBACK(gtk_widget_destroy), join_group_dialog);
    // Kết nối sự kiện để đóng hộp thoại khi người dùng nhấn nút thoát
    g_signal_connect_swapped(join_group_exit_btn, "clicked", G_CALLBACK(gtk_widget_destroy), join_group_dialog);
    // Kết nối sự kiện để xử lý tạo nhóm khi người dùng nhấn nút tạo nhóm
    g_signal_connect(join_group_create_btn, "clicked", G_CALLBACK(on_join_group_create_btn_clicked), client_socket_pt);
    // Kết nối sự kiện để xử lý tham gia nhóm khi người dùng nhấn nút tham gia nhóm
    g_signal_connect(join_group_join_btn, "clicked", G_CALLBACK(on_join_group_join_btn_clicked), client_socket_pt);

    // Hiển thị tất cả các thành phần trong hộp thoại
    gtk_widget_show_all(join_group_dialog);
}


// Hàm hiển thị hộp thoại mời vào nhóm
void show_invite_to_group_dialog(int *client_socket_pt) {

    builder = gtk_builder_new_from_file("../views/invite_to_group_dialog.glade");

    invite_to_group_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "invite_to_group_dialog"));
    invite_to_group_box = GTK_WIDGET(gtk_builder_get_object(builder, "invite_to_group_box"));
    invite_to_group_entry = GTK_WIDGET(gtk_builder_get_object(builder, "invite_to_group_entry"));
    invite_to_group_btn_box = GTK_WIDGET(gtk_builder_get_object(builder, "invite_to_group_btn_box"));
    invite_to_group_confirm_btn = GTK_WIDGET(gtk_builder_get_object(builder, "invite_to_group_confirm_btn"));
    invite_to_group_exit_btn = GTK_WIDGET(gtk_builder_get_object(builder, "invite_to_group_exit_btn"));

    // Kết nối sự kiện để đóng hộp thoại khi cửa sổ chính bị đóng
    g_signal_connect_swapped(main_window, "destroy", G_CALLBACK(gtk_widget_destroy), invite_to_group_dialog);
    // Kết nối sự kiện để đóng hộp thoại khi người dùng nhấn nút thoát
    g_signal_connect_swapped(invite_to_group_exit_btn, "clicked", G_CALLBACK(gtk_widget_destroy), invite_to_group_dialog);
    // Kết nối sự kiện để xử lý mời vào nhóm khi người dùng nhấn nút xác nhận
    g_signal_connect(invite_to_group_confirm_btn, "clicked", G_CALLBACK(on_invite_to_group_confirm_btn_clicked), client_socket_pt);

    // Hiển thị tất cả các thành phần trong hộp thoại
    gtk_widget_show_all(invite_to_group_dialog);
}


// Hàm hiển thị hộp thoại thông tin nhóm
void show_group_info_dialog() {

    builder = gtk_builder_new_from_file("../views/group_info_dialog.glade");

    group_info_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "group_info_dialog"));
    group_info_box = GTK_WIDGET(gtk_builder_get_object(builder, "group_info_box"));
    group_info_sw = GTK_WIDGET(gtk_builder_get_object(builder, "group_info_sw"));
    group_info_tv = GTK_WIDGET(gtk_builder_get_object(builder, "group_info_tv"));
    group_info_btn_box = GTK_WIDGET(gtk_builder_get_object(builder, "group_info_btn_box"));
    group_info_confirm_btn = GTK_WIDGET(gtk_builder_get_object(builder, "group_info_confirm_btn"));

    // Kết nối sự kiện để đóng hộp thoại khi cửa sổ chính bị đóng
    g_signal_connect_swapped(main_window, "destroy", G_CALLBACK(gtk_widget_destroy), group_info_dialog);
    // Kết nối sự kiện để đóng hộp thoại khi người dùng nhấn nút xác nhận
    g_signal_connect_swapped(group_info_confirm_btn, "clicked", G_CALLBACK(gtk_widget_destroy), group_info_dialog);

    // Hiển thị tất cả các thành phần trong hộp thoại
    gtk_widget_show_all(group_info_dialog);
}

// Hàm xử lý nhận dữ liệu từ server trong một luồng riêng biệt
gpointer recv_handler(gpointer data) {
    int *c_socket = (int *)data;
    int client_socket = *c_socket;
    
    Package pkg;
    while (1)
    {
        recv(client_socket, &pkg, sizeof(pkg), 0);
       
        // set the task to be not done yet
        is_done = 0;
       
        switch (pkg.ctrl_signal) {


            case SHOW_USER:
                // Xử lý khi nhận danh sách người dùng từ server và cập nhật giao diện
                gdk_threads_add_idle(recv_show_user, pkg.msg);
                break;

            case PRIVATE_CHAT:
                // Xử lý khi nhận tin nhắn riêng tư từ server và cập nhật giao diện
                gdk_threads_add_idle(recv_private_chat, &pkg);
                break;
            
            case SEND_PUBLIC_KEY:
                // Xử lý khi nhận yêu cầu gửi khóa công khai từ server và gửi khóa công khai của client
                receive_public_key(client_socket, &pkg);
                is_done = 1;
                break;
                       
            

            case ERR_INVALID_RECEIVER:
            // Xử lý khi có lỗi về người nhận không hợp lệ và thông báo lỗi
                make_done(ERR_INVALID_RECEIVER);
                gdk_threads_add_idle(recv_err_invalid_receiver, &pkg);
                break;
            case MSG_SENT_SUCC:
            // Xử lý khi tin nhắn đã được gửi thành công và thông báo thành công
                make_done(MSG_SENT_SUCC);
                gdk_threads_add_idle(recv_msg_sent_succ, &pkg);
                break;
           
            case SHOW_GROUP:
            // Xử lý khi nhận danh sách các nhóm từ server và cập nhật giao diện
                gdk_threads_add_idle(recv_show_group, pkg.msg);
                break;

            case MSG_MAKE_GROUP_SUCC:
            // Xử lý khi tạo nhóm thành công và thông báo thành công
                gdk_threads_add_idle(recv_make_group_succ, pkg.msg);
                break;
            case JOIN_GROUP_SUCC:
            // Xử lý khi tham gia nhóm thành công, cập nhật thông tin nhóm và thông báo thành công
                strcpy(curr_group_name, pkg.msg);
                curr_group_id = pkg.group_id;
                join_succ = 1;
                gdk_threads_add_idle(recv_join_group_succ, pkg.msg);
                break;
            case INVITE_FRIEND:
            // Xử lý khi nhận lời mời tham gia nhóm từ server và cập nhật giao diện
                gdk_threads_add_idle(recv_invite_friend, &pkg);
                break;
            case ERR_GROUP_NOT_FOUND:
            // Xử lý khi nhóm không tồn tại và thông báo lỗi
                gdk_threads_add_idle(recv_err_group_not_found, NULL);
                break;
            case ERR_IVITE_MYSELF:
            // Xử lý khi tự mời chính mình vào nhóm và thông báo lỗi
                gdk_threads_add_idle(recv_err_invite_myself, NULL);
                break;
            case ERR_USER_NOT_FOUND:
             // Xử lý khi người dùng không tồn tại và thông báo lỗi
                gdk_threads_add_idle(recv_err_user_not_found, NULL);
                break;
            case ERR_FULL_MEM:
            // Xử lý khi nhóm đã đầy và không thể tham gia, thông báo lỗ
                gdk_threads_add_idle(recv_err_full_mem, NULL);
                break;
            case ERR_IS_MEM:
            // Xử lý khi đã là thành viên của nhóm và không thể mời, thông báo lỗi
                gdk_threads_add_idle(recv_err_is_mem, NULL);
                break;
            case INVITE_FRIEND_SUCC:
            // Xử lý khi mời bạn bè vào nhóm thành công và thông báo thành công
                gdk_threads_add_idle(recv_invite_friend_succ, &pkg);
                break;
            case GROUP_CHAT:
            // Xử lý khi nhận tin nhắn từ nhóm và hiển thị nó trên giao diện
                gdk_threads_add_idle(recv_group_chat, &pkg);
               
                break;
            case SHOW_GROUP_INFO_START:
            // Xử lý khi bắt đầu nhận thông tin nhóm và chuẩn bị hiển thị
                gdk_threads_add_idle(recv_show_group_info_start, NULL);
                break;
            case SHOW_GROUP_NAME:
            // Xử lý khi nhận tên nhóm và chuẩn bị hiển thị
                gdk_threads_add_idle(recv_show_group_name, pkg.msg);
                break;
            case SHOW_GROUP_MEM_NUMBER:
             // Xử lý khi nhận số lượng thành viên trong nhóm và chuẩn bị hiển thị
                gdk_threads_add_idle(recv_show_group_mem_number, pkg.msg);
                break;
            case SHOW_GROUP_MEM_USERNAME:
            // Xử lý khi nhận danh sách thành viên trong nhóm và chuẩn bị hiển thị
                gdk_threads_add_idle(recv_show_group_mem_username, pkg.msg);
                break;
            case SHOW_GROUP_INFO_END:
            // Xử lý khi nhận thông tin nhóm hoàn thành và kết thúc quá trình hiển thị
                gdk_threads_add_idle(recv_show_group_info_end, NULL);
                break;
            case LEAVE_GROUP_SUCC:
             // Xử lý khi rời nhóm thành công và thông báo thành công
                gdk_threads_add_idle(recv_leave_group_succ, NULL);
                break;
            case LOG_OUT:
            // Đánh dấu thoát khỏi luồng khi người dùng đăng xuất
                g_thread_exit(NULL);
                break;
            default:
            // Trường hợp mặc định, đánh dấu tác vụ đã hoàn thành
                is_done = 1;
                break;
        }
        // wait for the task to be done
        while (!is_done);
    }

    return NULL;
}

//xử lý và hiển thị danh sách người dùng trực tuyến lên giao diện
gboolean recv_show_user(gpointer data) {

    // Nhận dữ liệu chứa danh sách người dùng từ data
    char *str = (char *) data;
    char text[USERNAME_SIZE];
    int i = -1;
    int j;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Xóa nội dung cũ trong GtkTextView để chuẩn bị cho việc thêm danh sách người dùng mới
    delete_textview_content(GTK_TEXT_VIEW(online_tv));

    // Vòng lặp để xử lý từng người dùng trong danh sách
    while (1) {

        // Đặt j bằng vị trí tiếp theo của i
        j = i + 1;
        // Duyệt qua các ký tự khoảng trắng hoặc kết thúc chuỗi để tìm vị trí bắt đầu của tên người dùng tiếp theo
        if (str[j] == ' ' || str[j] == '\0') break;

        // Tăng i để bắt đầu đếm tên người dùng từ vị trí tiếp theo
        i++;
        // Duyệt qua các ký tự của tên người dùng để xác định vị trí kết thúc của tên
        while (str[i] != ' ' && str[i] != '\0') i++;

        // Xóa bộ nhớ đệm text để chuẩn bị sao chép tên người dùng mới
        memset(text, '\0', sizeof(text));
        // Sao chép tên người dùng vào text
        strncpy(text, str + j, i - j);
        // Thêm tên người dùng vào GtkTextView
        insert_to_textview(GTK_TEXT_VIEW(online_tv), text);
        // Thêm dòng mới vào GtkTextView
        insert_to_textview(GTK_TEXT_VIEW(online_tv), NEWLINE);
        
        // Nếu str[i] là kết thúc chuỗi thì thoát khỏi vòng lặp
        if (str[i] == '\0') break;
    }

    // Cuối cùng, scroll GtkScrolledWindow để hiển thị danh sách người dùng mới
    while (gtk_events_pending()) gtk_main_iteration();
    scroll_window_to_bottom(GTK_SCROLLED_WINDOW(online_sw));

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}

gboolean recv_show_group(gpointer data) {

    // Nhận dữ liệu chứa danh sách nhóm từ data
    char *str = (char *) data;
    char text[USERNAME_SIZE];
    int i = -1;
    int j;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Xóa nội dung cũ trong GtkTextView để chuẩn bị cho việc thêm danh sách nhóm mới
    delete_textview_content(GTK_TEXT_VIEW(group_tv));

    // Vòng lặp để xử lý từng nhóm trong danh sách
    while (1) {

        // Đặt j bằng vị trí tiếp theo của i
        j = i + 1;
        // Duyệt qua các ký tự khoảng trắng hoặc kết thúc chuỗi để tìm vị trí bắt đầu của tên nhóm tiếp theo
        if (str[j] == ' ' || str[j] == '\0') break;

        // Tăng i để bắt đầu đếm tên nhóm từ vị trí tiếp theo
        i++;
        // Duyệt qua các ký tự của tên nhóm để xác định vị trí kết thúc của tên
        while (str[i] != ' ' && str[i] != '\0') i++;

        // Xóa bộ nhớ đệm text để chuẩn bị sao chép tên nhóm mới
        memset(text, '\0', sizeof(text));
        // Sao chép tên nhóm vào text
        strncpy(text, str + j, i - j);
        // Thêm tên nhóm vào GtkTextView
        insert_to_textview(GTK_TEXT_VIEW(group_tv), text);
        // Thêm dòng mới vào GtkTextView
        insert_to_textview(GTK_TEXT_VIEW(group_tv), NEWLINE);
        
        // Nếu str[i] là kết thúc chuỗi thì thoát khỏi vòng lặp
        if (str[i] == '\0') break;
    }

    // Cuối cùng, scroll GtkScrolledWindow để hiển thị danh sách nhóm mới
    while (gtk_events_pending()) gtk_main_iteration();
    scroll_window_to_bottom(GTK_SCROLLED_WINDOW(group_sw));

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}


gboolean recv_err_invalid_receiver(gpointer data) {

    // Ép kiểu con trỏ data sang kiểu Package để truy xuất thông tin từ gói tin
    Package *pkg_pt = (Package *) data;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Hiển thị thông báo lỗi với hộp thoại thông báo
    notif_dialog(GTK_WINDOW(main_window), INVALID_RECEIVER_NOTIF);

    // Nếu thông điệp trong gói tin không phải là TESTING_MSG, xóa dòng cuối cùng trong GtkTextView
    if (strcmp(pkg_pt->msg, TESTING_MSG) != 0) {
        delete_lastline_textview(GTK_TEXT_VIEW(recv_msg_tv));
    }

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}

//xử lý và cập nhật giao diện khi một tin nhắn được gửi thành công
gboolean recv_msg_sent_succ(gpointer data) {

    // Ép kiểu con trỏ data sang kiểu Package để truy xuất thông tin từ gói tin
    Package *pkg_pt = (Package *) data;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Kiểm tra nếu thông điệp trong gói tin là TESTING_MSG
    if (strcmp(pkg_pt->msg, TESTING_MSG) == 0) {

        // Nếu người dùng đang ở trong một phòng nhóm, rời khỏi phòng đó
        const gchar* receiver = gtk_label_get_text(GTK_LABEL(cur_chat_label));
        if(strcmp(pkg_pt->receiver, receiver) != 0){
            curr_group_id = -1;
            join_succ = 0;
            gtk_label_set_text(GTK_LABEL(cur_chat_label), (const gchar *) pkg_pt->receiver);
            delete_textview_content(GTK_TEXT_VIEW(recv_msg_tv));
        }
    }

    // Đặt trỏ chuột vào ô nhập liệu để người dùng có thể nhập ngay lập tức
    gtk_widget_grab_focus(send_entry);

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}

//xử lý và hiển thị tin nhắn riêng tư được nhận từ server
gboolean recv_private_chat(gpointer data) {

    Package *pkg_pt = (Package *) data;

    // Kiểm tra nếu thông điệp trong gói tin là TESTING_MSG
    if (strcmp(pkg_pt->msg, TESTING_MSG) == 0) {
        // Nếu là TESTING_MSG, đánh dấu task đã hoàn thành và kết thúc hàm
        is_done = 1;
        return FALSE;
    }
    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Nếu người dùng đang ở trong một phòng nhóm, rời khỏi phòng đó
    curr_group_id = -1;
    join_succ = 0;
    
    // Đếm số lượng phần tử trong mảng encrypted_msg để tránh truy cập out-of-bounds
    int i = 0;
    while(pkg_pt->encrypted_msg[i] != 0) {
        
        i++;
    }
    
    printf("Private Key:\n Modulus: %lld\n Exponent: %lld\n", (long long)my_priv->modulus, (long long)my_priv->exponent);
    // printf("%ld\n", sizeof(pkg_pt->encrypted_msg));

    // Giải mã tin nhắn sử dụng khóa riêng tư của người nhận
    char* decrypted = rsa_decrypt((long long*)pkg_pt->encrypted_msg, i * 8, my_priv);

    // Lấy nội dung của nhãn hiện tại đang được chọn
    const gchar *cur_chat_label_content = gtk_label_get_text(GTK_LABEL(cur_chat_label));

    // Nếu nhãn hiện tại không trùng với người gửi, cập nhật nhãn và xóa nội dung cũ
    if (strcmp(cur_chat_label_content, pkg_pt->sender) != 0) {
        gtk_label_set_text(GTK_LABEL(cur_chat_label), (const gchar *) pkg_pt->sender);
        delete_textview_content(GTK_TEXT_VIEW(recv_msg_tv));
    }
    
    // Nếu tin nhắn không phải từ chính người dùng, hiển thị nó trên TextView
    if (strcmp(my_username, pkg_pt->sender) != 0) {

        insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), pkg_pt->sender);
        insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), SPLITER);
        insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), decrypted);
        insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), NEWLINE);

        // Đảm bảo TextView tự động cuộn xuống cuối cùng
        while (gtk_events_pending()) gtk_main_iteration();
        scroll_window_to_bottom(GTK_SCROLLED_WINDOW(recv_msg_sw));
    }

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Đánh dấu task đã hoàn thành
    is_done = 1;
    return FALSE;
}

//xử lý và cập nhật giao diện khi người dùng tạo một nhóm thành công
gboolean recv_make_group_succ(gpointer data) {

    // Ép kiểu con trỏ data sang kiểu char để truy xuất thông tin từ dữ liệu
    char *group_name = (char *) data;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Chuỗi thông báo thành công khi tạo nhóm
    char text[TEXT_SIZE] = {0};
    strcpy(text, MAKE_GROUP_SUCC_NOTIF);
    strcat(text, group_name);

    // Hiển thị thông báo
    notif_dialog(GTK_WINDOW(main_window), (gchar *) &text);

    // Đặt trỏ chuột vào ô nhập liệu để người dùng có thể tham gia nhóm ngay lập tức
    gtk_widget_grab_focus(join_group_entry);

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}

//cập nhật giao diện khi người dùng tham gia vào một nhóm chat thành công
gboolean recv_join_group_succ(gpointer data) {

    // Ép kiểu con trỏ data sang kiểu char để truy xuất thông tin từ dữ liệu
    char *group_name = (char *) data;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Cập nhật nhãn hiển thị cuộc trò chuyện hiện tại với tên nhóm mới
    gtk_label_set_text(GTK_LABEL(cur_chat_label), (const gchar *) group_name);

    // Xóa nội dung cũ trong ô nhận tin nhắn
    delete_textview_content(GTK_TEXT_VIEW(recv_msg_tv));

    // In lịch sử chat
    view_chat_history();

    // Hiển thị tin nhắn chat lịch sử trong ô nhận tin nhắn
    for (int i = 3; i < (nrow + 1) * ncolumn; i++)
    {
        if (i % 3 == 1) {
            insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), resultp[i]);
            insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), SPLITER);
        }
        if (i % 3 == 2) {
            insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), resultp[i]);
            insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), NEWLINE);
        }
    }

    // Thông báo có tin nhắn mới
    insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), NEW_MESSAGES_NOTIF);
    insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), NEWLINE);

    // Cuộn tới cuối cùng của ô nhận tin nhắn
    while (gtk_events_pending()) gtk_main_iteration();
    scroll_window_to_bottom(GTK_SCROLLED_WINDOW(recv_msg_sw));

    // Giải phóng bảng kết quả và đóng cơ sở dữ liệu
    sqlite3_free_table(resultp);
    sqlite3_close(database);

    // Đặt trỏ chuột vào ô nhập liệu để người dùng có thể nhập tin nhắn ngay lập tức
    gtk_widget_grab_focus(send_entry);

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}

//hiển thị thông báo lỗi khi người dùng cố gắng tham gia vào một nhóm không tồn tại
gboolean recv_err_group_not_found(gpointer data) {

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Hiển thị thông báo lỗi khi nhóm không được tìm thấy
    notif_dialog(GTK_WINDOW(main_window), GROUP_NOT_FOUND_NOTIF);

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}

//hiển thị thông báo lỗi khi người dùng cố gắng mời chính họ vào một nhóm. 
gboolean recv_err_invite_myself(gpointer data) {
    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Hiển thị thông báo lỗi khi người dùng tự mình mời chính mình vào nhóm
    notif_dialog(GTK_WINDOW(main_window), "Không thể mời chính bạn vào nhóm.");

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}


// Hiển thị thông báo lỗi khi không tìm thấy người dùng trong hệ thống.
// Hàm này khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện, sau đó hiển thị thông báo lỗi
// với nội dung "Người dùng không được tìm thấy." sử dụng hàm notif_dialog. 
// Sau khi hoàn tất quá trình cập nhật giao diện, hàm mở khóa mutex và đặt is_done = 1 để thông báo rằng nhiệm vụ đã hoàn thành.
gboolean recv_err_user_not_found(gpointer data) {
    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Hiển thị thông báo lỗi khi không tìm thấy người dùng trong hệ thống
    notif_dialog(GTK_WINDOW(main_window), "Người dùng không được tìm thấy.");

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}


// Hiển thị thông báo lỗi khi nhóm đã đầy, không thể thêm thành viên mới.
gboolean recv_err_full_mem(gpointer data) {
    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Hiển thị thông báo lỗi khi nhóm đã đầy
    notif_dialog(GTK_WINDOW(main_window), "Nhóm đã đầy, không thể thêm thành viên mới.");

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}


// Hiển thị thông báo lỗi khi người dùng cố gắng tham gia một nhóm mà họ đã là thành viên.
gboolean recv_err_is_mem(gpointer data) {
    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Hiển thị thông báo lỗi khi người dùng là thành viên của nhóm đó
    notif_dialog(GTK_WINDOW(main_window), "Bạn đã là thành viên của nhóm.");

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}

// Xử lý khi mời bạn bè vào nhóm thành công
gboolean recv_invite_friend_succ(gpointer data) {
    // Ép kiểu con trỏ data sang kiểu Package để truy xuất thông tin từ gói tin
    Package *pkg_pt = (Package *) data;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Tạo thông báo thành công với tên người nhận lời mời và tên nhóm
    char text[TEXT_SIZE] = {0};
    sprintf(text, "Mời %s vào nhóm %s thành công.", pkg_pt->receiver, pkg_pt->msg);

    // Hiển thị thông báo
    notif_dialog(GTK_WINDOW(main_window), text);

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}


// Xử lý khi nhận lời mời kết bạn từ người khác
gboolean recv_invite_friend(gpointer data) {
    // Ép kiểu con trỏ data sang kiểu Package để truy xuất thông tin từ gói tin
    Package *pkg_pt = (Package *) data;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Tạo thông báo với tên người gửi lời mời và tên nhóm
    char text[TEXT_SIZE] = {0};
    sprintf(text, "Bạn nhận được lời mời kết bạn từ %s.\nNhóm: %s", pkg_pt->sender, pkg_pt->msg);

    // Hiển thị thông báo và tự động làm mới danh sách
    notif_dialog(GTK_WINDOW(main_window), text);
    gtk_button_clicked(GTK_BUTTON(refresh_list_btn));

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}


// Xử lý khi nhận tin nhắn trong nhóm
gboolean recv_group_chat(gpointer data) {
    // Ép kiểu con trỏ data sang kiểu Package để truy xuất thông tin từ gói tin
    Package *pkg_pt = (Package *) data;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Kiểm tra nếu đang ở trong một nhóm khác, cập nhật nhóm hiện tại và hiển thị lại lịch sử chat
    if (curr_group_id != pkg_pt->group_id) {
        join_succ = 1;
        curr_group_id = pkg_pt->group_id;

        char group_name[GROUP_NAME_SIZE] = {0};
        sprintf(group_name, "Group_%d", curr_group_id);
        gtk_label_set_text(GTK_LABEL(cur_chat_label), group_name);

        delete_textview_content(GTK_TEXT_VIEW(recv_msg_tv));

        // In lịch sử chat
        view_chat_history();

        int cell_num;
        if (strcmp(pkg_pt->sender, SERVER_SYSTEM_USERNAME) == 0) {
            cell_num = (nrow + 1) * ncolumn;
        } else {
            cell_num = (nrow + 1) * ncolumn - 3;
        }

        for (int i = 3; i < cell_num; i++) {
            if (i % 3 == 1) {
                insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), resultp[i]);
                insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), SPLITER);
            }
            if (i % 3 == 2) {
                insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), resultp[i]);
                insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), NEWLINE);
            }
        }

        insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), NEW_MESSAGES_NOTIF);
        insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), NEWLINE);

        sqlite3_free_table(resultp);
        sqlite3_close(database);
    }

    // Hiển thị tin nhắn trong nhóm
    insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), pkg_pt->sender);
    insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), SPLITER);
    insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), pkg_pt->msg);
    insert_to_textview(GTK_TEXT_VIEW(recv_msg_tv), NEWLINE);

    // Scroll xuống cuối cùng
    while (gtk_events_pending()) gtk_main_iteration();
    scroll_window_to_bottom(GTK_SCROLLED_WINDOW(recv_msg_sw));

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;
    
    return FALSE;
}


// Xử lý khi nhận yêu cầu hiển thị thông tin nhóm
gboolean recv_show_group_info_start(gpointer data) {
    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Hiển thị hộp thoại thông tin nhóm và làm cho nút xác nhận không khả dụng
    show_group_info_dialog();
    gtk_widget_set_sensitive(group_info_confirm_btn, FALSE);

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;
    
    return FALSE;
}


// Xử lý khi nhận thông báo kết thúc hiển thị thông tin nhóm
gboolean recv_show_group_info_end(gpointer data) {
    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Làm cho nút xác nhận trở nên khả dụng
    gtk_widget_set_sensitive(group_info_confirm_btn, TRUE);

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}


// Xử lý khi nhận thông báo hiển thị tên nhóm
gboolean recv_show_group_name(gpointer data) {
    // Ép kiểu con trỏ data sang kiểu chuỗi để truy xuất thông tin từ gói tin
    char *str = (char *) data;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Chèn thông báo về tên nhóm vào TextView
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), GROUP_NAME_INDICATOR);
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), SPLITER);
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), str);
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), NEWLINE);
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), NEWLINE);

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}


// Xử lý khi nhận thông báo hiển thị số lượng thành viên trong nhóm
gboolean recv_show_group_mem_number(gpointer data) {
    // Ép kiểu con trỏ data sang kiểu chuỗi để truy xuất thông tin từ gói tin
    char *str = (char *) data;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Chèn thông báo về số lượng thành viên trong nhóm vào TextView
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), GROUP_MEM_NUMBER_INDICATOR);
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), SPLITER);
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), str);
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), NEWLINE);
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), NEWLINE);

    // Chèn thông báo về thành viên trong nhóm vào TextView
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), GROUP_MEM_USERNAME_INDICATOR);
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), SPLITER);
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), NEWLINE);

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}


// Xử lý khi nhận thông báo hiển thị tên thành viên trong nhóm
gboolean recv_show_group_mem_username(gpointer data) {
    // Ép kiểu con trỏ data sang kiểu chuỗi để truy xuất thông tin từ gói tin
    char *str = (char *) data;

    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Chèn tên thành viên nhóm vào TextView
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), str);
    insert_to_textview(GTK_TEXT_VIEW(group_info_tv), NEWLINE);

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}


// Xử lý khi nhận thông báo rời khỏi nhóm thành công
gboolean recv_leave_group_succ(gpointer data) {
    // Khóa mutex để đảm bảo an toàn đa luồng khi cập nhật giao diện
    g_mutex_lock(&ui_mutex);

    // Đặt trạng thái rời khỏi nhóm và ID nhóm hiện tại
    join_succ = 0;
    curr_group_id = -1;

    // Đặt lại nhãn hiển thị cuộc trò chuyện hiện tại và xóa nội dung TextView
    gtk_label_set_text(GTK_LABEL(cur_chat_label), DEFAULT_CUR_CHAT_LABEL);
    delete_textview_content(GTK_TEXT_VIEW(recv_msg_tv));

    // Hiển thị thông báo rời khỏi nhóm thành công
    notif_dialog(GTK_WINDOW(main_window), LEAVE_GROUP_SUCC_NOTIF);

    // Kích hoạt sự kiện nhấp nút làm mới danh sách nhóm
    gtk_button_clicked(GTK_BUTTON(refresh_list_btn));

    // Mở khóa mutex để hoàn tất quá trình cập nhật giao diện
    g_mutex_unlock(&ui_mutex);

    // Task đã hoàn thành
    is_done = 1;

    return FALSE;
}



//* ----------------------- MAIN FUNCTION -----------------------
int main(int argc, char *argv[]) {

    // Khởi tạo socket của client
    int client_socket = connect_to_server();

    // Khởi tạo GTK
    gtk_init(&argc, &argv);

    // Khởi tạo GMutex
    g_mutex_init(&ui_mutex);

    // Hiển thị cửa sổ đăng nhập
    show_login_window(&client_socket);

    return 0;
}
