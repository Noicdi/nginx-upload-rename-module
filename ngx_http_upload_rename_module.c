#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/* 保存 .conf 中的配置项 */
typedef struct {
  ngx_flag_t upload_rename_enable;
} ngx_http_upload_rename_loc_conf_t;

typedef struct {
  ngx_str_t name;
  ngx_str_t type;
  ngx_str_t path;
  ngx_str_t md5;
  ngx_str_t size;
} upload_file_info_t;

typedef struct {
  ngx_str_t body;
  size_t count; // 记录已使用的数据位置
} http_request_body_t;

static http_request_body_t stat_body;

static void *
ngx_http_upload_rename_create_loc_conf(ngx_conf_t *cf);

static char *
ngx_http_upload_rename_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

/* 此方法需要绑定到必备配置项 */
static char *
ngx_http_upload_rename_enable(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_int_t
ngx_http_upload_rename_handler(ngx_http_request_t *r);

static ngx_int_t
ngx_module_upload_file_rename(ngx_http_request_t *r);

/* 获取 HTTP request body，并复制到 stat_body 申请的动态内存中 */
static void
ngx_module_get_http_request_body(ngx_http_request_t *r);

/* 根据 stat_body 提取上传文件的信息 */
static void
ngx_module_get_file_info(upload_file_info_t *info);

/* 根据 info 修改文件信息 */
static ngx_int_t
ngx_module_rename_file(ngx_http_request_t *r, upload_file_info_t info);

/* 模块配置项的详细信息 */
static ngx_command_t ngx_http_upload_rename_commands[] = {
    {ngx_string("upload_files_rename"),
     NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
     ngx_http_upload_rename_enable,
     NGX_HTTP_LOC_CONF_OFFSET,
     0,
     NULL},

    ngx_null_command};

/* 模块上下文，回调函数指针，指定合适时机执行的函数 */
static ngx_http_module_t ngx_http_upload_rename_module_ctx = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    ngx_http_upload_rename_create_loc_conf, // 调用该函数创建本模块位于location block的配置信息存储结构
    ngx_http_upload_rename_merge_loc_conf};

/* 模块定义 */
ngx_module_t ngx_http_upload_rename_module = {
    NGX_MODULE_V1,
    &ngx_http_upload_rename_module_ctx, // 模块上下文
    ngx_http_upload_rename_commands,    // 模块配置项
    NGX_HTTP_MODULE,                    // 模块类型
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING};

static void *
ngx_http_upload_rename_create_loc_conf(ngx_conf_t *cf)
{
  ngx_http_upload_rename_loc_conf_t *local_conf = NULL;

  local_conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_upload_rename_loc_conf_t));
  if (local_conf == NULL) {
    return NULL;
  }

  local_conf->upload_rename_enable = NGX_CONF_UNSET;

  return local_conf;
}

static char *
ngx_http_upload_rename_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
  ngx_http_upload_rename_loc_conf_t *prev = parent;
  ngx_http_upload_rename_loc_conf_t *conf = child;

  ngx_conf_merge_off_value(conf->upload_rename_enable, prev->upload_rename_enable, 0);

  return NGX_CONF_OK;
}

static char *
ngx_http_upload_rename_enable(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t *core_loc_conf;

  char *result = ngx_conf_set_flag_slot(cf, cmd, conf);

  if (result == NGX_CONF_OK) {
    core_loc_conf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    core_loc_conf->handler = ngx_http_upload_rename_handler;
  }

  return result;
}

static ngx_int_t
ngx_http_upload_rename_handler(ngx_http_request_t *r)
{
  ngx_http_upload_rename_loc_conf_t *loc_conf;
  ngx_int_t result = NGX_DECLINED;

  /* 获取 ngx_http_upload_rename_loc_conf */
  loc_conf = ngx_http_get_module_loc_conf(r, ngx_http_upload_rename_module);

  //ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "----- ngx_http_upload_rename_module is start! -----");

  /* echo_http_request == on */
  if (loc_conf->upload_rename_enable) {
    result = ngx_module_upload_file_rename(r);
  }

  //ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "----- ngx_http_upload_rename_module is end! -----");

  return result;
}

static ngx_int_t
ngx_module_upload_file_rename(ngx_http_request_t *r)
{
  if (r->method & NGX_HTTP_POST) {
    ngx_int_t rc;
    upload_file_info_t info;

    // 获取请求体并保存到 stat_body
    rc = ngx_http_read_client_request_body(r, ngx_module_get_http_request_body);
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
      return rc;
    }

    while (stat_body.count < stat_body.body.len) {
      // 获取文件信息
      ngx_module_get_file_info(&info);

      // 修改文件信息
      if (ngx_module_rename_file(r, info)) {
        ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "----- Failed to modify file name! -----");
      }
    }
  } else {
    //ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "----- The method of this HTTP request is not \"GET\" -----");
  }

  return NGX_DECLINED;
}

static void
ngx_module_get_http_request_body(ngx_http_request_t *r)
{
  if (ngx_exiting || ngx_terminate) {
    ngx_http_finalize_request(r, NGX_HTTP_CLOSE);
    return;
  }

  ngx_http_request_body_t *request_body;
  ngx_chain_t *part; // 单向链表的当前节点
  ngx_buf_t *buf;    // 当前节点的缓冲区
  u_char *p;

  request_body = r->request_body;

  if (request_body == NULL) {
    ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "----- This POST request has no body -----");
    ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    return;
  }

  stat_body.body.len = 0;

  for (part = request_body->bufs; part; part = part->next) {
    buf = part->buf;
    for (p = buf->pos; p < buf->last; p++) {
      stat_body.body.len++;
    }
  }

  // 拷贝 request body 的内容
  // 不清楚缓冲区是否连贯，所以采用申请内存空间的方式
  stat_body.body.data = ngx_pcalloc(r->pool, sizeof(char) * stat_body.body.len);
  if (stat_body.body.data == NULL) {
    ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "----- For HTTP request body fails to open dynamic memory -----");
  }

  stat_body.count = 0;

  for (part = request_body->bufs; part; part = part->next) {
    buf = part->buf;
    for (p = buf->pos; p < buf->last; p++) {
      *(stat_body.body.data + stat_body.count) = *p;
      stat_body.count++;
    }
  }

  stat_body.count = 0;

  ngx_http_finalize_request(r, NGX_DONE);

  return;
}

static void
ngx_module_get_file_info(upload_file_info_t *info)
{
  ngx_str_null(&info->name);
  ngx_str_null(&info->type);
  ngx_str_null(&info->path);
  ngx_str_null(&info->md5);
  ngx_str_null(&info->size);

  // 获取 file.name
  info->name.data = (u_char *)ngx_strstr(stat_body.body.data + stat_body.count, ".name\"\r\n") + 10;
  info->name.len = (size_t)((u_char *)ngx_strstr(info->name.data, "\r") - info->name.data);

  // 获取 file.type
  info->type.data = (u_char *)ngx_strstr(stat_body.body.data + stat_body.count, "_type\"\r\n") + 10;
  info->type.len = (size_t)((u_char *)ngx_strstr(info->type.data, "\r") - info->type.data);

  // 获取 file.path
  info->path.data = (u_char *)ngx_strstr(stat_body.body.data + stat_body.count, ".path\"\r\n") + 10;
  info->path.len = (size_t)((u_char *)ngx_strstr(info->path.data, "\r") - info->path.data);

  // 获取 file.md5
  info->md5.data = (u_char *)ngx_strstr(stat_body.body.data + stat_body.count, ".md5\"\r\n") + 9;
  info->md5.len = (size_t)((u_char *)ngx_strstr(info->md5.data, "\r") - info->md5.data);

  // 获取 file.size
  info->size.data = (u_char *)ngx_strstr(stat_body.body.data + stat_body.count, ".size\"\r\n") + 10;
  info->size.len = (size_t)((u_char *)ngx_strstr(info->size.data, "\r") - info->size.data);

  stat_body.count = (size_t)(info->size.data + info->size.len - stat_body.body.data + 60);

  return;
}

static ngx_int_t
ngx_module_rename_file(ngx_http_request_t *r, upload_file_info_t info)
{
  if (info.name.len == 0 || info.path.len == 0) {
    return -1;
  }

  ngx_int_t result;
  ngx_str_t full_name;
  ngx_str_t path;
  path.len = info.path.len;
  path.data = info.path.data;

  while (*(path.data + path.len - 1) != '/') {
    path.len--;
  }
  full_name.len = path.len + info.name.len;
  full_name.data = ngx_pcalloc(r->pool, sizeof(char) * (full_name.len + 1));

  size_t count = 0;
  for (size_t i = 0; i < path.len; i++) {
    *(full_name.data + count) = *(path.data + i);
    count++;
  }
  for (size_t i = 0; i < info.name.len; i++) {
    *(full_name.data + count) = *(info.name.data + i);
    count++;
  }
  *(full_name.data + full_name.len) = '\0';

  path.data = ngx_pcalloc(r->pool, sizeof(char) * (info.path.len + 1));
  for (count = 0; count < info.path.len; count++) {
    *(path.data + count) = *(info.path.data + count);
  }
  *(path.data + count) = '\0';

  result = ngx_rename_file(path.data, full_name.data);

  return result;
}
