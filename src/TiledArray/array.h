#ifndef TILEDARRAY_ARRAY_H__INCLUDED
#define TILEDARRAY_ARRAY_H__INCLUDED

#include <TiledArray/distributed_array.h>
#include <TiledArray/tiled_range.h>
#include <TiledArray/tile.h>
#include <TiledArray/annotated_array.h>
#include <TiledArray/array_util.h>
#include <TiledArray/transform_iterator.h>
#include <TiledArray/madness_runtime.h>
#include <TiledArray/array_ref.h>
#include <boost/shared_ptr.hpp>
#include <boost/functional.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <functional>

namespace TiledArray {

  // Forward declaration of TiledArray Permutation.
  template<unsigned int DIM>
  class Permutation;
  template<typename I, unsigned int DIM, typename CS>
  class TiledRange;
  template<typename T, unsigned int DIM, typename CS>
  class Tile;
  template<typename T, typename I>
  class BaseArray;
  template <typename T, unsigned int DIM, typename CS>
  class Array;
  template<typename T, typename I, unsigned int DIM>
  BaseArray<T, I>* operator^=(BaseArray<T, I>*, const Permutation<DIM>&);
  template<typename T, unsigned int DIM, typename CS>
  void swap(Array<T, DIM, CS>&, Array<T, DIM, CS>&);

  namespace expressions {
    class VariableList;
//    namespace tile {
//      template<typename T>
//      class AnnotatedTile;
//
//    } // namespace tile
//
//    namespace array {
//
//      template<typename T>
//      class AnnotatedTile;
//    } // namespace array
  } // namespace expressions



  /// Array interface class.

  /// Provides a common interface for math operations on array objects.
  template<typename T, typename I>
  class BaseArray {
  private:
    typedef BaseArray<T, I> BaseArray_;

    // Prevent copy operations.
    BaseArray(const BaseArray<T, I>&);
    BaseArray<T, I>& operator=(const BaseArray<T, I>&);

  protected:
    // Basic typedefs
    typedef typename boost::remove_const<T>::type value_type;
    typedef I ordinal_type;
    typedef I volume_type;
    typedef detail::ArrayRef<const I> size_array;

    // Annotated tile typedefs
    typedef expressions::tile::AnnotatedTile<value_type> atile_type;
    typedef expressions::tile::AnnotatedTile<const value_type> const_atile_type;
    typedef std::pair<ordinal_type, atile_type> data_type;
    typedef std::pair<ordinal_type, const_atile_type> const_data_type;
    typedef madness::Future<data_type> fut_atile_type;
    typedef madness::Future<const_data_type> const_fut_atile_type;

    // Iterator typedefs
    typedef detail::PolyTransformIterator<fut_atile_type> iterator_atile;
    typedef detail::PolyTransformIterator<const_fut_atile_type> const_iterator_atile;

    BaseArray() { }
    virtual ~BaseArray() { }

    // Clone the array
    virtual BaseArray_* clone(bool copy_data = false) const = 0;

    // Iterators which return futures to annotated tiles.
    virtual iterator_atile begin_atile(const expressions::VariableList&) = 0;
    virtual const_iterator_atile begin_atile(const expressions::VariableList&) const = 0;
    virtual iterator_atile end_atile(const expressions::VariableList&) = 0;
    virtual const_iterator_atile end_atile(const expressions::VariableList&) const = 0;

    // Basic array modification interface.
    virtual void insert(const std::size_t, const value_type*, const value_type*) = 0;
    virtual void insert(const std::size_t, const detail::ArrayRef<value_type>&) = 0;
    virtual void insert(const std::size_t, const atile_type&) = 0;
    virtual void erase(const std::size_t) = 0;

    // Returns information on the array tiles.
    virtual bool is_local(const std::size_t) const = 0;
    virtual bool includes(const std::size_t) const = 0;
    virtual detail::ArrayRef<const value_type> data(const std::size_t) const = 0;
    virtual detail::ArrayRef<value_type> data(const std::size_t) = 0;
    virtual size_array size_ref() const = 0;
    virtual size_array weight_ref() const = 0;
    virtual void permute(const ordinal_type*) = 0;

    // Remote communication
    virtual madness::Future<bool> probe(const ordinal_type) const = 0;
    virtual fut_atile_type find(const ordinal_type, const expressions::VariableList& v) = 0;
    virtual const_fut_atile_type find(const ordinal_type, const expressions::VariableList& v) const = 0;

  public:
    // public access functions.
    virtual void clear() = 0;
    virtual volume_type volume(bool local = false) const = 0;
    virtual madness::World& get_world() const = 0;

  private:
    friend class expressions::array::AnnotatedArray<T>;
  }; // class BaseArray

  template<typename T, typename I, unsigned int DIM>
  BaseArray<T, I>* operator^=(BaseArray<T, I>* a, const Permutation<DIM>& p) {
    a->permute(p.begin(), p.end());
    return a;
  }

  /// Tiled Array with data distributed across many nodes.
  template <typename T, unsigned int DIM, typename CS = CoordinateSystem<DIM> >
  class Array : public BaseArray<T, std::size_t>, public madness::WorldObject<Array<T, DIM, CS> > {
  public:
    typedef Array<T, DIM, CS> Array_;
    typedef BaseArray<T, std::size_t> BaseArray_;
    typedef madness::WorldObject<Array<T, DIM, CS> > WorldObject_;
    typedef CS coordinate_system;
    typedef Tile<T, DIM, CS> tile_type;

    static unsigned int dim() { return DIM; }

  private:
    typedef DistributedArray<tile_type, DIM, LevelTag<1>, coordinate_system> data_container;

  public:
    typedef typename data_container::key_type key_type;
    typedef typename data_container::index_type index_type;
    typedef typename tile_type::index_type tile_index_type;
    typedef typename data_container::ordinal_type ordinal_type;
    typedef typename data_container::volume_type volume_type;
    typedef typename data_container::size_array size_array;
    typedef typename data_container::value_type value_type;
    typedef TiledRange<ordinal_type, DIM, CS> tiled_range_type;
    typedef typename data_container::accessor accessor;
    typedef typename data_container::const_accessor const_accessor;
    typedef typename data_container::iterator iterator;
    typedef typename data_container::const_iterator const_iterator;
    typedef Range<ordinal_type, DIM, LevelTag<1>, coordinate_system > range_type;

  private:

    // Prohibited operations
    Array();
    Array(const Array_&);
    Array_ operator=(const Array_&);

  public:
    /// creates an array living in world and described by shape. Optional
    /// val specifies the default value of every element
    Array(madness::World& world, const tiled_range_type& rng) :
        WorldObject_(world), range_(rng), tiles_(world, rng.tiles().size())
    {
      this->process_pending();
    }

    /// AnnotatedArray copy constructor
    template<typename U>
    Array(const expressions::array::AnnotatedArray<U>& aarray) :
        WorldObject_(aarray.get_world())
    {
      // TODO: Implement this function
      TA_ASSERT(false, std::runtime_error, "Not yet implemented.");
      TA_ASSERT((aarray.dim() == DIM), std::runtime_error,
          "The dimensions of the annotated tile do not match the dimensions of the tile.");
      this->process_pending();
    }

#ifdef __GXX_EXPERIMENTAL_CXX0X__
    /// AnnotatedArray copy constructor
    template<typename U>
    Array(expressions::array::AnnotatedArray<U>&& aarray) : BaseArray_(aarray.get_world())
    {
      // TODO: Implement this function.
      TA_ASSERT(false, std::runtime_error, "Not yet implemented.");
      TA_ASSERT((aarray.dim() == DIM), std::runtime_error,
          "The dimensions of the annotated array do not match the dimensions of the tile.");
      this->process_pending();
    }
#endif // __GXX_EXPERIMENTAL_CXX0X__

    /// Destructor function
    virtual ~Array() {}

    /// Copy the content of the other array into this array.

    /// Performs a deep copy of this array into the other array. The content of
    /// the other array will be deleted. This function is blocking and may cause
    /// some communication.
    void clone(const Array_& other) {
      range_ = other.range_;
      tiles_.clone(other.tiles_);
    }

    /// Inserts a tile into the array.

    /// Inserts a tile with all elements initialized to a constant value.
    /// Non-local insertions will initiate non-blocking communication.
    template<typename Key>
    void insert(const Key& k, T value = T()) {
      tile_type t(range_.tile(key_(k)), value);
      tiles_.insert(key_(k), t);
    }

    /// Inserts a tile into the array.

    /// Inserts a tile with all elements initialized to the values given by the
    /// iterator range [first, last). Non-local insertions will initiate
    /// non-blocking communication.
    template<typename Key, typename InIter>
    void insert(const Key& k, InIter first, InIter last) {
      BOOST_STATIC_ASSERT(detail::is_input_iterator<InIter>::value);
      tile_type t(range_.tile(key_(k)), first, last);
      tiles_.insert(key_(k), t);
    }

    /// Inserts a tile into the array.

    /// Copies the given tile into the array. Non-local insertions will initiate
    /// non-blocking communication.
    template<typename Key>
    void insert(const Key& k, const tile_type& t) {
      TA_ASSERT(t.size() == range_.tile(key_(k)).size(), std::runtime_error,
          "Tile boundaries do not match array tile boundaries.");
      tiles_.insert(key_(k), t);
    }

    /// Inserts a tile into the array.

    /// Copies the given value_type into the array. Non-local insertions will
    /// initiate non-blocking communication.
    template<typename Key>
    void insert(const std::pair<Key, tile_type>& v) {
      insert(v.first, v.second);
    }

    /// Erases a tile from the array.

    /// This will remove the tile at the given index. It will initiate
    /// non-blocking for non-local tiles.
    template<typename Key>
    void erase(const Key& k) {
      tiles_.erase(key_(k));
    }

    /// Erase a range of tiles from the array.

    /// This will remove the range of tiles from the array. The iterator must
    /// dereference to value_type (std::pair<index_type, tile_type>). It will
    /// initiate non-blocking communication for non-local tiles.
    template<typename InIter>
    void erase(InIter first, InIter last) {
      BOOST_STATIC_ASSERT(detail::is_input_iterator<InIter>::value);
      for(; first != last; ++first)
        tiles_.erase(key_(first->first));
    }

    /// Removes all tiles from the array.
    virtual void clear() {
      tiles_.clear();
    }

    /// Returns an iterator to the first local tile.
    iterator begin() { return tiles_.begin(); }
    /// returns a const_iterator to the first local tile.
    const_iterator begin() const { return tiles_.begin(); }
    /// Returns an iterator to the end of the local tile list.
    iterator end() { return tiles_.end(); }
    /// Returns a const_iterator to the end of the local tile list.
    const_iterator end() const { return tiles_.end(); }

    /// Resizes the array to the given tiled range.

    /// The array will be resized to the given dimensions and tile boundaries.
    /// This will erase all data contained by the array.
    void resize(const tiled_range_type& r) {
      range_ = r;
      tiles_.resize(range_.tiles().size(), false);
    }

    /// Permutes the array. This will initiate blocking communication.
    Array_& operator ^=(const Permutation<DIM>& p) {
      for(iterator it = begin(); it != end(); ++it)
        it->second ^= p; // permute the individual tile
      range_ ^= p;
      tiles_ ^= p; // move the tiles to the correct location. Blocking communication here.

      return *this;
    }

    /// Returns true if the tile specified by index is stored locally.
    template<typename Key>
    bool is_local(const Key& k) const {
      return tiles_.is_local(key_(k));
    }

    /// Returns true if the element specified by tile index i is stored locally.
    bool is_local(const tile_index_type& i) const {
      return is_local(get_tile_index_(i));
    }

    /// Returns the index of the lower tile boundary.
    const index_type& start() const {
      return range_.tiles().start();
    }

    /// Returns the index  of the upper tile boundary.
    const index_type& finish() const {
      return range_.tiles().finish();
    }

    /// Returns a reference to the array's size array.
    const size_array& size() const {
      return tiles_.size();
    }

    /// Returns a reference to the dimension weight array.
    const size_array& weight() const {
      return tiles_.weight();
    }

    /// Returns the number of elements present in the array.

    /// If local == false, then the total number of tiles in the array will be
    /// returned. Otherwise, if local == true, it will return the number of
    /// tiles that are stored locally. The number of local tiles may or may not
    /// reflect the maximum possible number of tiles that can be stored locally.
    virtual volume_type volume(bool local = false) const {
      return tiles_.volume(local);
    }

    /// Returns true if the tile is included in the array range.
    template<typename Key>
    bool includes(const Key& k) const {
      return tiles_.includes(key_(k));
    }

    /// Returns a Future iterator to an element at index i.

    /// This function will return an iterator to the element specified by index
    /// i. If the element is not local the it will use non-blocking communication
    /// to retrieve the data. The future will be immediately available if the data
    /// is local.
    template<typename Key>
    madness::Future<iterator> find(const Key& k) {
      return tiles_.find(key_(k));
    }

    /// Returns a Future const_iterator to an element at index i.

    /// This function will return a const_iterator to the element specified by
    /// index i. If the element is not local the it will use non-blocking
    /// communication to retrieve the data. The future will be immediately
    /// available if the data is local.
    template<typename Key>
    madness::Future<const_iterator> find(const Key& k) const {
      return tiles_.find(key_(k));
    }

    /// Sets an accessor to point to a local data element.

    /// This function will set an accessor to point to a local data element only.
    /// It will return false if the data element is remote or not found.
    template<typename Key>
    bool find(accessor& acc, const Key& k) {
      return tiles_.find(acc, key_(k));
    }

    /// Sets a const_accessor to point to a local data element.

    /// This function will set a const_accessor to point to a local data element
    /// only. It will return false if the data element is remote or not found.
    template<typename Key>
    bool find(const_accessor& acc, const Key& k) const {
      return tiles_.find(acc, key_(k));
    }

    virtual madness::World& get_world() const { return WorldObject_::get_world(); }

    expressions::array::AnnotatedArray<T> operator ()(const std::string& v) {
      return expressions::array::AnnotatedArray<T>(this, expressions::VariableList(v));
    }

    expressions::array::AnnotatedArray<const T> operator ()(const std::string& v) const {
      return expressions::array::AnnotatedArray<const T>(this, expressions::VariableList(v));
    }

    /// Returns a reference to the tile range object.
    const typename tiled_range_type::range_type& tiles() const {
      return range_.tiles();
    }

    /// Returns a reference to the element range object.
    const typename tiled_range_type::element_range_type& elements() const {
      return range_.elements();
    }

    /// Returns a reference to the specified tile range object.
    const typename tiled_range_type::tile_range_type& tile(const index_type& i) const {
      return range_.tile(i);
    }

  protected:

    virtual BaseArray_* clone(bool copy_data = true) const {
      Array_* result = new Array_(this->world, range_);
      if(copy_data)
        for(const_iterator it = begin(); it != end(); ++it)
          result->insert(*it);

      return result;
    }
    template<typename U, typename Arg>
    struct MakeFutATile
    {
    private:
      MakeFutATile();
    public:
      typedef std::pair<typename key_type::key1_type, expressions::tile::AnnotatedTile<U> > data_type;
      typedef Arg argument_type;
      typedef madness::Future<data_type> result_type;

      MakeFutATile(const expressions::VariableList& var) : var_(var) { }
      result_type operator()(argument_type t) const {
        return result_type(data_type(t.first.key1(), t.second(var_)));
      }

    private:
      const expressions::VariableList& var_;
    }; // struct MakeFutATile

    // Iterators which return annotated tiles when dereferenced.
    virtual typename BaseArray_::iterator_atile
    begin_atile(const expressions::VariableList& v) {
      return typename BaseArray_::iterator_atile(begin(),
          MakeFutATile<T, typename iterator::value_type&>(v));
    }
    virtual typename BaseArray_::const_iterator_atile
    begin_atile(const expressions::VariableList& v) const {
      return typename BaseArray_::const_iterator_atile(begin(),
          MakeFutATile<const T, const typename iterator::value_type&>(v));
    }
    virtual typename BaseArray_::iterator_atile
    end_atile(const expressions::VariableList& v) {
      return typename BaseArray_::iterator_atile(end(),
          MakeFutATile<T, typename iterator::value_type&>(v));
    }
    virtual typename BaseArray_::const_iterator_atile
    end_atile(const expressions::VariableList& v) const {
      return typename BaseArray_::const_iterator_atile(end(),
          MakeFutATile<const T, const typename iterator::value_type&>(v));
    }

    /// Inserts a tile at the give ordinal index with the given data values.
    virtual void insert(const ordinal_type i, const typename BaseArray_::value_type* first, const typename BaseArray_::value_type* last) {
      tile_type t(range_.tile(i), first, last);
      tiles_.insert(i, t);
    }

    virtual void insert(const ordinal_type i, const detail::ArrayRef<typename BaseArray_::value_type>& a) {
      tile_type t(range_.tile(i), a.begin(), a.end());
      tiles_.insert(i, t);
    }

    virtual void insert(const ordinal_type i, const typename BaseArray_::atile_type& a) {
      tile_type t(range_.tile(i), a.begin(), a.end());
      tiles_.insert(i, t);
    }

    /// Erases a tile at the given ordinal index.
    virtual void erase(const std::size_t i) {
      erase(i);
    }

    /// Returns true if the ordinal index is stored locally.
    virtual bool is_local(const std::size_t i) const {
      return tiles_.is_local(get_index_(i));
    }

    /// Returns true if the ordinal index is included in the array.
    virtual bool includes(const std::size_t i) const {
      return i < tiles_.volume();
    }

    /// Returns a pair of pointers that point to the indicated tile's data.
    virtual detail::ArrayRef<typename BaseArray_::value_type> data(const std::size_t i) {
      index_type index = get_index_(i);
      accessor acc;
      find(acc, index);
      typename BaseArray_::value_type* p = acc->second.data();
      return detail::ArrayRef<typename BaseArray_::value_type>(p, p + acc->second.volume());
    }

    /// Returns a pair of pointers that point to the indicated tile's data.
    virtual detail::ArrayRef<const typename BaseArray_::value_type> data(const std::size_t i) const {
      index_type index = get_index_(i);
      const_accessor acc;
      find(acc, index);
      const typename BaseArray_::value_type* p = acc->second.data();
      return detail::ArrayRef<const typename BaseArray_::value_type>(p, p + acc->second.volume());
    }



    /// Return the a pair of pointers to the size of the array.
    virtual typename BaseArray_::size_array size_ref() const {
      return typename BaseArray_::size_array(tiles().size().begin(), tiles().size().end());
    }

    /// Return the a pair of pointers to the weight of the array.
    virtual typename BaseArray_::size_array weight_ref() const {
      return typename BaseArray_::size_array(tiles_.weight().begin(), tiles_.weight().end());
    }

    virtual void permute(const std::size_t* first) {
      Permutation<DIM> p(first);
      for(iterator it = begin(); it != end(); ++it)
        it->second ^= p; // permute the individual tile
      range_ ^= p;
      tiles_ ^= p; // move the tiles to the correct location. Blocking communication here.
    }

    /// Sends a bool indicating the existence of a tile to a specified process.

    /// This function is called via an active message, and will send a bool to
    /// the requestor. The result will be true if the tile exists on the local
    /// node and false otherwise.
    /// \var \c requester is the process where the tile will be sent
    /// \var \c i is the ordinal index of the tile being probed
    /// \var \c ref is the remote reference to the destination future
    madness::Void send_probe(ProcessID requester, const ordinal_type i,
        const madness::RemoteReference< madness::FutureImpl<bool> >& ref) const
    {
      madness::Future<const_iterator> fit = find(i);
      const bool result = fit.get() != end();
      this->send(requester, &Array_::receive_probe, ref, result);
      return madness::None;
    }

    /// Handles successful find response
    madness::Void receive_probe(const madness::RemoteReference< madness::FutureImpl<bool> >& ref, const bool p) {
      madness::FutureImpl<bool>* f = ref.get();
      f->set(p);
      ref.dec();
      return madness::None;
    }

    /// Sends a tile to a specified process.

    /// This function is called via an active message, and will send the tile at
    /// ordinal index to the requesting process.
    /// \var \c requester is the process where the tile will be sent
    /// \var \c i is the ordinal index of the tile to be sent
    /// \var \c ref is the remote reference to the destination future
    /// \var \c var is the variable list which will be used to construct the annotated tile
    madness::Void send_atile(ProcessID requester, const ordinal_type i,
        const madness::RemoteReference< madness::FutureImpl<typename BaseArray_::data_type> >& ref,
        const expressions::VariableList& var)
    {
      // Todo: We need to find a way to eliminate the need to send the variable list with the active message.
      accessor acc;
      if(find(acc, i))
        this->send(requester, &Array_::receive_atile, ref, i, acc->second, var);
      else
        this->send(requester, &Array_::receive_no_atile, ref, i);
      return madness::None;
    }

    /// Handles successful find response
    madness::Void receive_atile(const madness::RemoteReference< madness::FutureImpl<typename BaseArray_::data_type> >& ref, const ordinal_type i, const tile_type& tile, const expressions::VariableList& var) {
      madness::FutureImpl<typename BaseArray_::data_type>* f = ref.get();
      f->set(typename BaseArray_::data_type(i, typename BaseArray_::atile_type(tile.size(),
          var, tile.begin(), tile.end(), coordinate_system::dimension_order)));
      ref.dec();
      return madness::None;
    }

    /// Handles unsuccessful find response
    madness::Void receive_no_atile(const madness::RemoteReference< madness::FutureImpl<typename BaseArray_::data_type> >& ref, const ordinal_type i) {
      madness::FutureImpl<typename BaseArray_::data_type>* f = ref.get();
      f->set(typename BaseArray_::data_type(i, typename BaseArray_::atile_type()));
      ref.dec();
      return madness::None;
    }

    /// Sends a tile to a specified process.

    /// This function is called via an active message, and will send the tile at
    /// ordinal index to the requesting process.
    /// \var \c requester is the process where the tile will be sent
    /// \var \c i is the ordinal index of the tile to be sent
    /// \var \c ref is the remote reference to the destination future
    /// \var \c var is the variable list which will be used to construct the annotated tile
    madness::Void send_const_atile(ProcessID requester, const ordinal_type i,
        const madness::RemoteReference< madness::FutureImpl<typename BaseArray_::const_data_type> >& ref,
        const expressions::VariableList& var)
    {
      // Todo: We need to find a way to eliminate the need to send the variable list with the active message.
      const_accessor acc;
      if(find(acc, i))
        this->send(requester, &Array_::receive_const_atile, ref, i, acc->second, var);
      else
        this->send(requester, &Array_::receive_no_const_atile, ref, i);
      return madness::None;
    }

    /// Handles successful find response
    madness::Void receive_const_atile(const madness::RemoteReference< madness::FutureImpl<typename BaseArray_::const_data_type> >& ref,
        const ordinal_type i, const tile_type& tile, const expressions::VariableList& var)
    {
      madness::FutureImpl<typename BaseArray_::const_data_type>* f = ref.get();
      f->set(typename BaseArray_::const_data_type(i,
          typename BaseArray_::const_atile_type(tile.size(), var, tile.begin(),
          tile.end(), coordinate_system::dimension_order)));
      ref.dec();
      return madness::None;
    }

    /// Handles unsuccessful find response
    madness::Void receive_no_const_atile(const madness::RemoteReference< madness::FutureImpl<typename BaseArray_::const_data_type> >& ref,
        const ordinal_type i)
    {
      madness::FutureImpl<typename BaseArray_::const_data_type>* f = ref.get();
      f->set(typename BaseArray_::const_data_type(i, typename BaseArray_::const_atile_type()));
      ref.dec();
      return madness::None;
    }

    // Remote communication
    virtual madness::Future<bool> probe(const ordinal_type i) const {
      const ProcessID dest = tiles_.owner(i);
      const ProcessID me = this->world.mpi.rank();
      madness::Future<bool> result;
      if(dest == me) {
        const_iterator it = tiles_.find(i);
        result.set(it != tiles_.end());
      } else {
        this->send(dest, &Array_::send_probe, me, i, result.remote_ref(this->world));
      }

      return result;
    }

    virtual typename BaseArray_::fut_atile_type find(const ordinal_type i, const expressions::VariableList& v) {
      const ProcessID dest = tiles_.owner(i);
      const ProcessID me = this->world.mpi.rank();
      typename BaseArray_::fut_atile_type result;
      if (dest == me) {
        accessor acc;
        tiles_.find(acc, i);
        result.set(typename BaseArray_::data_type(i, acc->second(v)));
        return result;
      } else {
        this->send(dest, &Array_::send_atile, me, i, result.remote_ref(this->world), v);
      }

      return result;
    }

    virtual typename BaseArray_::const_fut_atile_type find(const ordinal_type i, const expressions::VariableList& v) const {
      const ProcessID dest = tiles_.owner(i);
      const ProcessID me = this->world.mpi.rank();
      typename BaseArray_::const_fut_atile_type result;
      if (dest == me) {
        const_accessor acc;
        tiles_.find(acc, i);
        result.set(typename BaseArray_::const_data_type(i, acc->second(v)));
      } else {
        this->send(dest, &Array_::send_const_atile, me, i, result.remote_ref(this->world), v);
      }

      return result;
    }

  private:

    /// Returns the tile index that contains the element index e_idx.
    index_type get_tile_index_(const tile_index_type& i) const {
      return * range_.find(i);
    }

    /// Converts an ordinal into an index
    index_type get_index_(const ordinal_type i) const {
      index_type result;
      detail::calc_index(i, coordinate_system::rbegin(tiles_.weight()),
          coordinate_system::rend(tiles_.weight()),
          coordinate_system::rbegin(result));
      return result;
    }

    const ordinal_type& key_(const ordinal_type& o) const {
      return o;
    }

    const index_type key_(const index_type& i) const {
      return i - start();
    }

    const ordinal_type key_(const key_type& k) const {
      return k.key1();
    }

    ordinal_type ord_(const index_type& i) const {
      return std::inner_product(i.begin(), i.end(), tiles_.weight().begin(),
          ordinal_type(0));
    }

    friend void swap<>(Array_&, Array_&);

    tiled_range_type range_;
    data_container tiles_;
  }; // class Array

  template<typename T, unsigned int DIM, typename CS>
  void swap(Array<T, DIM, CS>& a0, Array<T, DIM, CS>& a1) {
    TiledArray::swap(a0.range_, a1.range_);
    TiledArray::swap(a0.tiles_, a1.tiles_);
  }

} // namespace TiledArray

#endif // TILEDARRAY_ARRAY_H__INCLUDED
