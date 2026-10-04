## The problem

Design the database schema and API for a simple e-commerce application: users browse
products, add them to a cart, and place orders.

Cover, in your answer:

- A DB schema for Users, Products, Cart, and Orders -- table names, key columns, and how
  they relate to each other.
- The API surface: the endpoints (or RPC methods) needed to browse products, manage a
  cart, and place an order, with enough detail on request/response shape to show you've
  thought about who calls what, when.
- What happens to an order's line items once it's placed -- why an order shouldn't just
  point at the current `products` row for its price and description.
- What stops two requests from both decrementing the same product's stock below zero when
  two customers buy the last unit at the same time.
- Normalization: where you'd normalize (and why) versus where you'd deliberately
  denormalize for read performance, for this specific schema.

There's no code to write here. Answer in plain writing, the way you'd talk it through on a
whiteboard.
