option dialect quickr
type Res
    name as string

    fn tag(self) as string
        return self.name
    end fn

    fn drop(mut self)
        print "drop "; self.name
    end fn
end type
