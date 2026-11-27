option dialect quickr
type Shelf[T]
    things as T[]
    private count as int

    fn put(mut self, x as T)
        self.things.push(x)
        self.count += 1
    end fn

    fn count_of(self) as int
        return self.count
    end fn
end type
