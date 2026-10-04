### How it's called

```cpp
TicketBooking booking(100);   // 100 seats

std::atomic<int> winners{0};
std::vector<std::thread> buyers;
for (int user = 0; user < 50; ++user) {
    buyers.emplace_back([&, user]{
        if (booking.reserve(/*seat=*/5, user)) winners.fetch_add(1);   // all 50 target seat 5
    });
}
for (int user = 0; user < 20; ++user)
    buyers.emplace_back([&, user]{ booking.reserve(/*seat=*/12, user); });  // unrelated seat

for (auto& t : buyers) t.join();
// winners.load() == 1 -- exactly one of the 50 seat-5 buyers won
// booking.owner_of(5) is that winning user_id; booking.owner_of(12) is whoever won seat 12
```
