A good answer covers:

- **A schema with the right entities and keys.** `users(id, email, ...)`,
  `products(id, name, price, stock_count, ...)`, `carts(id, user_id)` +
  `cart_items(cart_id, product_id, quantity)`, `orders(id, user_id, status, total,
  created_at)` + `order_items(order_id, product_id, quantity, unit_price_at_purchase)`.
  A good answer gets the many-to-many shape right for cart/order line items (a join table,
  not a repeated-column or JSON-blob hack) and names the foreign keys explicitly.
- **Order line items snapshot price and product details at purchase time, not a live
  reference.** `order_items.unit_price_at_purchase` (and ideally product name) is stored
  on the order row itself, not looked up fresh from `products` every time the order is
  displayed. Names the reason: if the product's price changes next week, a past order's
  total must not change retroactively -- an order is a historical record, not a live view.
  This is the single most commonly-missed point in this question.
- **An API surface organized around the actual resources and actions**, roughly:
  `GET /products`, `GET /products/:id`, `POST /cart/items`, `DELETE /cart/items/:id`,
  `POST /orders` (checkout, converts the current cart into an order), `GET /orders/:id`.
  A good answer thinks about idempotency for `POST /orders` specifically (a network retry
  on checkout shouldn't create two orders) -- an idempotency key passed by the client is
  the standard fix, worth naming even briefly.
- **Stock decrement is atomic, not check-then-write.** `UPDATE products SET stock_count =
  stock_count - 1 WHERE id = ? AND stock_count > 0`, checking the affected row count,
  instead of reading `stock_count`, checking it in application code, and writing the
  decrement in a second step -- the classic race where two concurrent checkouts both read
  stock=1 and both proceed. A good answer names this explicitly when asked, not just "we
  check the stock before allowing checkout."
- **A normalization stance with a reason, not just "normalize everything."** Users,
  products, and the cart/order join tables are normalized (no duplicated product data
  scattered across rows). The order-line-item snapshot above is a deliberate, justified
  denormalization (a copy of price/name at time of purchase) -- a good answer explicitly
  frames it that way rather than treating it as an oversight or an afterthought.

NEEDS_WORK if the answer never distinguishes "current product data" from "product data as
it was when the order was placed" (treats `order_items` as just referencing `products`
live), or never addresses how stock is decremented safely under concurrent checkouts, or
produces no actual schema (table names, columns, keys) and stays purely verbal.

## Code

**Order line items pointing live at products -- an order's total silently changes if the
product's price changes later:**
```sql
CREATE TABLE order_items (
  order_id INT REFERENCES orders(id),
  product_id INT REFERENCES products(id),
  quantity INT
  -- no price column -- total gets computed by joining to products.price at display time
);
```

**Order line items snapshot price at purchase, immune to later product changes:**
```sql
CREATE TABLE order_items (
  order_id INT REFERENCES orders(id),
  product_id INT REFERENCES products(id),
  quantity INT,
  unit_price_at_purchase NUMERIC NOT NULL,
  product_name_at_purchase TEXT NOT NULL
);
```

**Stock decrement race vs. an atomic one:**
```sql
-- race: two checkouts can both pass this check when stock_count = 1
SELECT stock_count FROM products WHERE id = ?;         -- both read 1
UPDATE products SET stock_count = stock_count - 1 WHERE id = ?;  -- both succeed -- stock now -1

-- atomic: the WHERE clause is the check, only one checkout wins
UPDATE products SET stock_count = stock_count - 1
WHERE id = ? AND stock_count > 0;
-- affected rows == 0 means "sold out", tell that checkout to fail cleanly
```
