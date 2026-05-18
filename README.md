# Online Shop — TCP Socket (C)

A client-server online bookshop implemented in C using POSIX TCP sockets, built for COS3105 Operating Systems.

---

## Features

- Multi-client server using `fork()` per connection
- Shared memory (`mmap`) so all child processes share the same store, cart, and user data
- Role-based menus: **Anonymous**, **Member**, **Admin**
- bcrypt password hashing via `libcrypt`
- Persistent user accounts (file-based), in-memory store and cart state

---

## Project Structure

```
miniproject/
├── minipServer.c       Server — accepts connections, dispatches commands
├── minipClient.c       Client — menu-driven terminal UI
├── auth.c              User registration, login, session management
├── store.c             Product CRUD (add, update, delete, search, list)
├── order.c             Cart management and order history
├── common.h            Shared structs, constants, and status codes
├── test_all.c          Unit + integration tests (97 assertions)
├── conclude.txt        Project conclusion and known limitations
├── Makefile
└── txt/
    ├── user.txt        Persistent user accounts (username,hash,isAdmin)
    ├── store.txt       Initial product catalog
    └── order.txt       Persisted order history
```

---

## Build & Run

### Requirements
- GCC
- `libcrypt` (`sudo apt install libssl-dev` on Ubuntu)

### Server
```bash
make s
# or manually:
gcc -o s minipServer.c -lcrypt
./s
```

### Client (in a separate terminal)
```bash
make c
# or manually:
gcc -o c minipClient.c
./c
```

### Tests
```bash
make test
# or manually:
gcc -o test_all test_all.c -lcrypt
./test_all
```

> The server listens on port **8082** by default.

---

## Usage

### Anonymous Menu
| Option | Action |
|--------|--------|
| 1 | View all products |
| 2 | Search product by ID |
| 3 | Login |
| 4 | Register |
| 0 | Exit |

### Member Menu (after login)
| Option | Action |
|--------|--------|
| 1 | View all products |
| 2 | Search product by ID |
| 3 | Add to cart |
| 4 | View cart |
| 5 | Checkout |
| 6 | View order history |
| 7 | Logout |

### Admin Menu (after admin login)
| Option | Action |
|--------|--------|
| 1 | Manage products → Add / Update / Remove |
| 2 | View all orders |
| 3 | View all members |
| 7 | Logout |

---

## Admin Accounts

Admin accounts are seeded directly in `txt/user.txt`. They cannot be created through the client.

**Default credentials:**
| Username | Password |
|----------|----------|
| `admin_user` | `admin123` |
| `admin` | `admin123` |

To add your own admin, register a normal member through the client, then edit `txt/user.txt` and change the last field from `0` → `1`. Restart the server to apply changes.

---

## Protocol

All communication uses a plain-text command protocol over TCP.

```
Request:   COMMAND,arg1,arg2,...
Response:  COMMAND,STATUS[,data]
```

Data items are separated by `|`. Fields within an item are separated by `-`.

**Example:**
```
Client → VIEW_PRODUCT
Server ← VIEW_PRODUCT,0,COS1101-Intro to CS-30.50-12|COS3105-OS-10.50-10|
```

### Status Codes
| Code | Meaning |
|------|---------|
| 0 | OK |
| 1 | Fail |
| 100 | Invalid session |
| 101 | Permission denied |
| 102 | Duplicate user |
| 200 | Invalid arguments |
| 201 | Product not found |
| 204 | Empty cart |

---

## Architecture

```
┌────────────┐  TCP :8082   ┌─────────────────────────────────┐
│   Client   │ ──────────▶  │  Server (parent)                │
│minipClient │ ◀──────────  │  accept() → fork() per client   │
└────────────┘              └──────────┬──────────────────────┘
                                       │ mmap(MAP_SHARED)
                            ┌──────────▼──────────────────────┐
                            │  Shared Memory                  │
                            │  ┌──────────┐ ┌──────┐ ┌─────┐ │
                            │  │UserSess. │ │Store │ │Order│ │
                            │  └──────────┘ └──────┘ └─────┘ │
                            └─────────────────────────────────┘
```

Each client connection gets its own forked child process. All child processes share the same `UserSessions`, `Store`, and `Order` via anonymous shared memory, so a login from one child is immediately visible to all others.

---

## Known Limitations

- Store and cart changes are **not persisted** to file on exit — data is lost when the server stops
- `MAX_CART = 10` limits total cart entries across **all users**
- No TLS — credentials are sent as plaintext over TCP
- Admin handlers have no server-side permission check — security relies on the client menu

---

## License

Academic project — COS3105 Operating Systems
